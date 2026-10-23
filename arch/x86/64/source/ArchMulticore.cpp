#include "ArchMulticore.h"
#include "ACPI.h"
#include "ArchCpuLocalStorage.h"
#include "assert.h"
#include "kstring.h"
#include "new.h"
#include "APIC.h"
#include "ArchMemory.h"
#include "ArchInterrupts.h"
#include "kprintf.h"
#include "Scheduler.h"

__cpu CPU* currentCPU;
CPU* bootCPU;
static CPU* cpus;
extern SegmentDescriptor gdt[7];
extern uint8 boot_stack[];
static CPU* stopping_cpu;
static size_t tlb_generation;

Scheduler* ArchMulticore::nextScheduler()
{
  static size_t next_cpu;
  assert(system_state == RUNNING);
  size_t index = ArchThreads::atomic_add(next_cpu, 1) % ACPI::num_cpus;
  assert(__atomic_load_n(&cpus[index].online, __ATOMIC_ACQUIRE) && cpus[index].scheduler);
  return cpus[index].scheduler;
}

struct InterruptFrame;

extern "C" __attribute__((interrupt)) void arch_stopHandler(InterruptFrame*)
{
  assert(__atomic_load_n(&stopping_cpu, __ATOMIC_ACQUIRE) && "Unexpected NMI");
  __atomic_store_n(&currentCPU->stopped, true, __ATOMIC_RELEASE);
  while (true)
    asm volatile("hlt"); // NMI entry disables interrupts; this CPU never resumes.
}

extern "C" __attribute__((interrupt)) void arch_tlbHandler(InterruptFrame*)
{
  ++outstanding_EOIs;
  // Coalesced IPIs are fine: this flush covers every generation published before it.
  size_t generation = __atomic_load_n(&tlb_generation, __ATOMIC_ACQUIRE);
  ArchMemory::flushTlb();
  APIC::sendEOI();
  __atomic_store_n(&currentCPU->tlb_generation, generation, __ATOMIC_RELEASE);
}

void ArchMulticore::stopOtherCPUs()
{
  ArchInterrupts::disableInterrupts();
  if (!cpus || !currentCPU)
    return;
  CPU* expected = nullptr;
  if (!__atomic_compare_exchange_n(&stopping_cpu, &expected, currentCPU, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
  {
    if (expected == currentCPU)
      return; // A recursive panic on the CPU already stopping the others.
    while (true)
      asm volatile("hlt"); // A simultaneous panic waits for the winning CPU's NMI.
  }
  for (size_t i = 0; i < ACPI::num_cpus; ++i)
    if (&cpus[i] != currentCPU && __atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE))
      APIC::sendIPI(cpus[i].apic_id, 4 << 8); // NMI delivery; the vector field is ignored.
  for (size_t i = 0; i < ACPI::num_cpus; ++i)
  {
    if (&cpus[i] == currentCPU || !__atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE))
      continue;
    size_t attempts = 100000000;
    while (!__atomic_load_n(&cpus[i].stopped, __ATOMIC_ACQUIRE) && --attempts)
      asm volatile("pause");
    assert(attempts && "CPU did not stop");
  }
}

void ArchMulticore::invalidateTLBs()
{
  if (system_state != RUNNING)
  {
    ArchMemory::flushTlb();
    return;
  }
  assert(ArchInterrupts::testIFSet() && "TLB shootdowns must be able to receive other CPUs' IPIs");
  // Publish our page-table changes and acknowledge locally without an IPI interrupting between the two.
  ArchInterrupts::disableInterrupts();
  size_t generation = __atomic_add_fetch(&tlb_generation, 1, __ATOMIC_SEQ_CST);
  assert(generation && "TLB generation wrapped");
  ArchMemory::flushTlb();
  __atomic_store_n(&currentCPU->tlb_generation, generation, __ATOMIC_RELEASE);
  ArchInterrupts::enableInterrupts();
  for (size_t i = 0; i < ACPI::num_cpus; ++i)
    if (&cpus[i] != currentCPU && __atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&cpus[i].stopped, __ATOMIC_ACQUIRE))
      APIC::sendIPI(cpus[i].apic_id, 66); // Fixed delivery, TLB-invalidation vector.
  for (size_t i = 0; i < ACPI::num_cpus; ++i)
  {
    if (&cpus[i] == currentCPU || !__atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE))
      continue;
    size_t attempts = 100000000;
    while (__atomic_load_n(&cpus[i].tlb_generation, __ATOMIC_ACQUIRE) < generation &&
           !__atomic_load_n(&cpus[i].stopped, __ATOMIC_ACQUIRE) && --attempts)
      asm volatile("pause");
    assert(attempts && "CPU did not invalidate its TLB");
  }
}

static void initialiseCPU(CPU* cpu)
{
  assert(!currentCPU);
  currentCPU = cpu;
  memcpy(cpu->gdt, gdt, sizeof(cpu->gdt));
  cpu->tss.rsp0 = (size_t)cpu->stack + 0x4000;
  cpu->tss.iomap = sizeof(cpu->tss); // No userspace access to I/O ports.

  size_t address = (size_t)&cpu->tss;
  SegmentDescriptor& tss_descriptor = cpu->gdt[KERNEL_TSS / sizeof(SegmentDescriptor)];
  tss_descriptor.baseLL = address;
  tss_descriptor.baseLM = address >> 16;
  tss_descriptor.baseLH = address >> 24;
  tss_descriptor.baseH = address >> 32;
  tss_descriptor.limitL = sizeof(cpu->tss) - 1;
  tss_descriptor.limitH = 0;
  tss_descriptor.typeL = 0x89; // Present, available 64-bit TSS; LTR marks it busy.
  tss_descriptor.typeH = 0;

  const struct
  {
    uint16 limit;
    size_t base;
  } __attribute__((packed)) gdtr = {sizeof(cpu->gdt) - 1, (size_t)cpu->gdt};
  asm volatile("lgdt %0\n"
               "ltr %w1" : : "m"(gdtr), "r"((uint16)KERNEL_TSS) : "memory");
}

void ArchMulticore::initialise()
{
  cpus = new CPU[ACPI::num_cpus]{};
  uint32 eax, ebx, ecx, edx;
  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
  uint8 boot_apic_id = ebx >> 24;
  for (size_t i = 0; i < ACPI::num_cpus; ++i)
  {
    cpus[i].apic_id = ACPI::cpu_apic_ids[i];
    if (cpus[i].apic_id == boot_apic_id)
    {
      cpus[i].stack = boot_stack;
      initialiseCPU(&cpus[i]);
      cpus[i].online = true;
    }
  }
  assert(currentCPU && "Boot CPU is missing from the MADT");
  bootCPU = currentCPU;
  bootCPU->scheduler = Scheduler::instance();
}

extern "C" uint8 ap_boot_start[], ap_boot_end[], ap_boot_parameters[];
extern "C" void arch_contextSwitch();
static PageMapLevel4Entry ap_root[PAGE_MAP_LEVEL_4_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static struct
{
  uint16 limit;
  size_t base;
} __attribute__((packed)) ap_idtr;

struct APBootParameters
{
  uint32 cr3;
  uint32 reserved;
  size_t stack;
  CPU* cpu;
  void (*entry)(CPU*);
};
static_assert(sizeof(APBootParameters) == 32);

extern "C" __attribute__((noreturn)) void apEntry(CPU* cpu)
{
  CPULocalStorage::initialise(cpu->tls);
  assert(!currentCPU && !currentThread && !currentThreadRegisters);
  uint32 eax, ebx, ecx, edx;
  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
  assert((ebx >> 24) == cpu->apic_id && !ArchInterrupts::testIFSet());
  initialiseCPU(cpu);
  currentThreadRegisters = &cpu->registers;
  size_t root = (size_t)VIRTUAL_TO_PHYSICAL_BOOT(ArchMemory::getRootOfKernelPagingStructure());
  asm volatile("mov %0, %%cr3\n"
               "lidt %1" : : "r"(root), "m"(ap_idtr) : "memory");

  assert(currentCPU == cpu && currentThreadRegisters == &cpu->registers);
  size_t stack_address;
  asm volatile("mov %%rsp, %0" : "=r"(stack_address));
  assert(stack_address >= (size_t)cpu->stack && stack_address < (size_t)cpu->stack + 0x4000);
  cpu->scheduler = Scheduler::instance(); // Construct this CPU's scheduler before reporting it ready.
  APIC::initialiseLocal();
  __atomic_store_n(&cpu->online, true, __ATOMIC_RELEASE);
  // Locks are bypassed during boot; wait until the boot CPU finishes allocating for all CPUs.
  while (__atomic_load_n(&system_state, __ATOMIC_ACQUIRE) != RUNNING)
    asm volatile("pause");
  APIC::enableTimer();
  Scheduler::instance()->schedule();
  arch_contextSwitch(); // IRET enables interrupts on the selected thread's stack.
  __builtin_unreachable();
}

void ArchMulticore::startOtherCPUs()
{
  assert(!ArchInterrupts::testIFSet());
  const size_t entry_address = 0x8000;
  size_t code_size = ap_boot_end - ap_boot_start;
  assert(code_size <= PAGE_SIZE);
  uint8* entry = (uint8*)ArchMemory::getIdentAddressOfPPN(entry_address / PAGE_SIZE);
  memcpy(entry, ap_boot_start, code_size);
  APBootParameters* parameters = (APBootParameters*)(entry + (ap_boot_parameters - ap_boot_start));

  memcpy(ap_root, ArchMemory::getRootOfKernelPagingStructure(), PAGE_SIZE);
  ap_root[0] = ap_root[480]; // Temporary low mapping for real-mode code, removed on entry to C++.
  asm volatile("sidt %0" : "=m"(ap_idtr));
  CPU* boot_cpu = currentCPU;
  ArchThreadRegisters* boot_registers = currentThreadRegisters;
  for (size_t i = 0; i < ACPI::num_cpus; ++i)
  {
    CPU* cpu = &cpus[i];
    if (cpu == boot_cpu)
      continue;
    cpu->tls = new uint8[CPULocalStorage::size()];
    cpu->stack = new uint8[0x4000];
    *parameters = {(uint32)(size_t)VIRTUAL_TO_PHYSICAL_BOOT(ap_root), 0,
                   (size_t)cpu->stack + 0x4000, cpu, apEntry};
    APIC::startCPU(cpu->apic_id, entry_address);
    size_t attempts = 100000000;
    while (!__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE) && --attempts)
      asm volatile("pause");
    assert(attempts && "Secondary CPU did not finish startup");
    assert(currentCPU == boot_cpu && currentThreadRegisters == boot_registers && !currentThread);
    kprintfd("CPU %u online: private TLS %p, stack %p, GDT %p, TSS %p\n",
             cpu->apic_id, cpu->tls, cpu->stack, cpu->gdt, &cpu->tss);
  }
}
