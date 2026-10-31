#include "APIC.h"
#include "ACPI.h"
#include "ArchInterrupts.h"
#include "ArchMemory.h"
#include "assert.h"
#include "paging-definitions.h"
#include "offsets.h"
#include "ports.h"

__cpu volatile size_t outstanding_EOIs;

static PageTableEntry apic_page_table[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static volatile uint32* local_apic;
static volatile uint32* io_apic;
static uint32 max_redirection;

static uint32 readIOAPIC(uint8 reg)
{
  io_apic[0] = reg; // IOREGSEL selects the register accessed through IOWIN.
  return io_apic[0x10 / 4];
}

static void writeIOAPIC(uint8 reg, uint32 value)
{
  io_apic[0] = reg;
  io_apic[0x10 / 4] = value;
}

static void mapRegisters()
{
  // Reserve one page table immediately below the framebuffer mappings.
  extern PageDirEntry kernel_page_directory[];
#ifdef CMAKE_X86_64
  PageDirEntry* directory = kernel_page_directory + PAGE_DIR_ENTRIES;
  const size_t base = 0xFFFFFFFFBEC00000;
#else
  PageDirEntry* directory = kernel_page_directory;
  const size_t base = 0xBEC00000;
#endif
  size_t index = base / (PAGE_SIZE * PAGE_TABLE_ENTRIES);
#ifdef CMAKE_X86_64
  index %= PAGE_DIR_ENTRIES;
#endif
  assert(!directory[index].pt.present);
#ifdef CMAKE_X86_64
  directory[index].pt.page_ppn = (size_t)VIRTUAL_TO_PHYSICAL_BOOT(apic_page_table) / PAGE_SIZE;
#else
  directory[index].pt.page_table_ppn = VIRTUAL_TO_PHYSICAL_BOOT((size_t)apic_page_table) / PAGE_SIZE;
#endif
  directory[index].pt.writeable = 1;
  directory[index].pt.present = 1;
  assert(!(ACPI::local_apic_address % PAGE_SIZE) && !(ACPI::io_apic_address % PAGE_SIZE));
  for (size_t i = 0; i < 2; ++i)
  {
    apic_page_table[i].page_ppn = (i == 0 ? ACPI::local_apic_address : ACPI::io_apic_address) / PAGE_SIZE;
    apic_page_table[i].writeable = 1;
    apic_page_table[i].cache_disabled = 1; // MMIO must not be cached like RAM.
    apic_page_table[i].write_through = 1;
    apic_page_table[i].present = 1;
  }
  asm volatile("invlpg (%0)\n"
               "invlpg (%1)" : : "r"(base), "r"(base + PAGE_SIZE) : "memory");
  local_apic = (volatile uint32*)base;
  io_apic = (volatile uint32*)(base + PAGE_SIZE);
}

void APIC::initialiseLocal()
{
  assert(!ArchInterrupts::testIFSet());
  uint32 low, high;
  asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1B)); // IA32_APIC_BASE.
  assert(!(low & (1 << 10)) && "x2APIC mode is not supported");
  assert((low & 0xFFFFF000) == ACPI::local_apic_address);
  low |= 1 << 11; // Globally enable the local APIC.
  asm volatile("wrmsr" : : "c"(0x1B), "a"(low), "d"(high));

  local_apic[0x80 / 4] = 0; // Task priority: accept all interrupt priorities.
  local_apic[0x350 / 4] = 1 << 16; // Mask LINT0 (legacy PIC input).
  local_apic[0x360 / 4] = 1 << 16; // Mask LINT1.
  local_apic[0x370 / 4] = 1 << 16; // Mask APIC error interrupts.
  local_apic[0xF0 / 4] = (1 << 8) | 127; // Enable APIC, spurious vector 127.
  local_apic[0x3E0 / 4] = 3; // Timer divide configuration: divide by 16.
  local_apic[0x320 / 4] = (1 << 17) | (1 << 16) | 32; // Periodic timer, initially masked, vector 32.
  local_apic[0x380 / 4] = 0x500000; // Scheduling interval in APIC ticks, not a calibrated duration.
}

void APIC::initialise()
{
  assert(!ArchInterrupts::testIFSet());
  uint32 eax, ebx, ecx, edx;
  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
  assert((edx & (1 << 9)) && "CPU requires a local APIC");
  ACPI::initialise();
  mapRegisters();

  initialiseLocal();

  max_redirection = (readIOAPIC(1) >> 16) & 0xFF;
  assert(max_redirection <= 119); // Redirection registers use an 8-bit selector.
  for (uint32 i = 0; i <= max_redirection; ++i)
    writeIOAPIC(0x10 + 2 * i, 1 << 16); // Mask all inputs before installing routes.

  uint32 destination = local_apic[0x20 / 4]; // Local APIC ID in bits 24..31.
  for (uint32 irq = 1; irq < 16; ++irq)
  {
    if (irq == 2) // No PIC cascade, and IRQ0 is replaced by the local APIC timer.
      continue;
    const ACPI::IRQ& route = ACPI::isa_irqs[irq];
    assert(route.global_interrupt >= ACPI::io_apic_interrupt_base);
    uint32 pin = route.global_interrupt - ACPI::io_apic_interrupt_base;
    assert(pin <= max_redirection);
    uint32 polarity = route.flags & 3;
    uint32 trigger = (route.flags >> 2) & 3;
    assert(polarity != 2 && trigger != 2); // Reserved MADT encodings.
    uint32 entry = (1 << 16) | (32 + irq); // Masked, fixed delivery, physical destination.
    if (polarity == 3)
      entry |= 1 << 13; // Active low; ISA default is active high.
    if (trigger == 3)
      entry |= 1 << 15; // Level triggered; ISA default is edge triggered.
    writeIOAPIC(0x11 + 2 * pin, destination);
    writeIOAPIC(0x10 + 2 * pin, entry);
  }
}

static void maskIRQ(uint16 irq, bool masked)
{
  assert(irq > 0 && irq < 16 && irq != 2);
  bool enabled = ArchInterrupts::disableInterrupts();
  uint32 pin = ACPI::isa_irqs[irq].global_interrupt - ACPI::io_apic_interrupt_base;
  assert(pin <= max_redirection);
  uint8 reg = 0x10 + 2 * pin;
  uint32 entry = readIOAPIC(reg);
  writeIOAPIC(reg, masked ? entry | (1 << 16) : entry & ~(1 << 16));
  if (enabled)
    ArchInterrupts::enableInterrupts();
}

void APIC::enableIRQ(uint16 irq)
{
  maskIRQ(irq, false);
}

void APIC::disableIRQ(uint16 irq)
{
  maskIRQ(irq, true);
}

void APIC::enableTimer()
{
  local_apic[0x320 / 4] &= ~(1 << 16);
}

void APIC::disableTimer()
{
  local_apic[0x320 / 4] |= 1 << 16;
}

void APIC::sendEOI()
{
  --outstanding_EOIs;
  local_apic[0xB0 / 4] = 0;
}

static void startupDelay(uint16 ticks)
{
  // PIT channel 2 counts at 1,193,182 Hz independently of interrupt delivery.
  uint8 gate = inportb(0x61);
  outportb(0x61, gate & ~3); // Disable the gate and speaker while programming.
  outportb(0x43, 0xB0); // Channel 2, 16-bit count, mode 0 (one shot).
  outportb(0x42, ticks & 0xFF);
  outportb(0x42, ticks >> 8);
  outportb(0x61, (gate & ~2) | 1); // Open the gate; keep the speaker off.
  size_t attempts = 10000000;
  while (!(inportb(0x61) & 0x20) && --attempts)
    asm volatile("pause");
  assert(attempts && "PIT startup delay did not complete");
  outportb(0x61, gate);
}

void APIC::sendIPI(uint8 apic_id, uint32 command)
{
  bool enabled = ArchInterrupts::disableInterrupts();
  local_apic[0x310 / 4] = (uint32)apic_id << 24;
  local_apic[0x300 / 4] = command;
  size_t attempts = 10000000;
  while ((local_apic[0x300 / 4] & (1 << 12)) && --attempts)
    asm volatile("pause");
  assert(attempts && "APIC IPI delivery did not complete");
  if (enabled)
    ArchInterrupts::enableInterrupts();
}

void APIC::startCPU(uint8 apic_id, uint32 entry_address)
{
  assert(!ArchInterrupts::testIFSet());
  assert(entry_address < 0x100000 && entry_address % PAGE_SIZE == 0);
  sendIPI(apic_id, 0xC500); // INIT, level triggered, asserted.
  startupDelay(11932); // 10 ms for the CPU to reset.
  sendIPI(apic_id, 0x8500); // Deassert INIT.
  sendIPI(apic_id, 0x600 | (entry_address / PAGE_SIZE)); // SIPI selects the real-mode entry page.
  startupDelay(239); // 200 us before the second SIPI.
  sendIPI(apic_id, 0x600 | (entry_address / PAGE_SIZE));
}
