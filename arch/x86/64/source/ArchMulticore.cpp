#include "ArchMulticore.h"
#include "ACPI.h"
#include "ArchCpuLocalStorage.h"
#include "assert.h"
#include "kstring.h"
#include "new.h"

__cpu CPU* currentCPU;
static CPU* cpus;
extern SegmentDescriptor gdt[7];
extern uint8 boot_stack[];

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
}
