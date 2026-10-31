#include "ACPI.h"
#include "ArchMemory.h"
#include "assert.h"
#include "kstring.h"
#include "kprintf.h"

uint32 ACPI::local_apic_address;
uint32 ACPI::io_apic_address;
uint32 ACPI::io_apic_interrupt_base;
uint8 ACPI::cpu_apic_ids[256];
size_t ACPI::num_cpus;
ACPI::IRQ ACPI::isa_irqs[16];

static const void* physicalAddress(uint32 address, size_t length)
{
  // The existing direct mapping covers the first GiB on all x86 targets.
  assert(address < 0x40000000 && length <= 0x40000000 - address);
  return (const uint8*)ArchMemory::getIdentAddressOfPPN(address / PAGE_SIZE) + (address % PAGE_SIZE);
}

static bool checksumValid(const void* data, size_t length)
{
  uint8 sum = 0;
  const uint8* bytes = (const uint8*)data;
  for (size_t i = 0; i < length; ++i)
    sum += bytes[i];
  return sum == 0;
}

bool ACPISDTHeader::checksumValid() const
{
  return ::checksumValid(this, Length);
}

static const RSDPDescriptor* checkForRSDP(uint32 start, uint32 end)
{
  for (uint32 address = start; address + sizeof(RSDPDescriptor) <= end; address += 16)
  {
    const RSDPDescriptor* rsdp = (const RSDPDescriptor*)physicalAddress(address, sizeof(RSDPDescriptor));
    if (memcmp(rsdp->Signature, "RSD PTR ", 8) == 0 && checksumValid(rsdp, sizeof(*rsdp)))
      return rsdp;
  }
  return nullptr;
}

static const ACPISDTHeader* tableAt(uint32 address)
{
  const ACPISDTHeader* header = (const ACPISDTHeader*)physicalAddress(address, sizeof(ACPISDTHeader));
  assert(header->Length >= sizeof(*header));
  physicalAddress(address, header->Length);
  assert(header->checksumValid() && "Invalid ACPI table checksum");
  return header;
}

static void parseMADT(const ACPI_MADTHeader* madt)
{
  assert(madt->header.Length >= sizeof(*madt));
  ACPI::local_apic_address = madt->local_apic_address;
  const uint8* cursor = (const uint8*)(madt + 1);
  const uint8* end = (const uint8*)madt + madt->header.Length;
  while (cursor != end)
  {
    assert((size_t)(end - cursor) >= sizeof(MADTEntryDescriptor));
    const MADTEntryDescriptor* entry = (const MADTEntryDescriptor*)cursor;
    assert(entry->length >= sizeof(*entry) && entry->length <= end - cursor);
    switch (entry->type)
    {
      case 0: // Processor local APIC.
      {
        assert(entry->length == sizeof(MADTProcLocalAPIC));
        const MADTProcLocalAPIC* cpu = (const MADTProcLocalAPIC*)entry;
        if (cpu->flags & 1) // Only processors enabled by firmware.
        {
          assert(ACPI::num_cpus < sizeof(ACPI::cpu_apic_ids));
          ACPI::cpu_apic_ids[ACPI::num_cpus++] = cpu->apic_id;
        }
        break;
      }
      case 1: // I/O APIC.
      {
        assert(entry->length == sizeof(MADT_IO_APIC));
        const MADT_IO_APIC* io = (const MADT_IO_APIC*)entry;
        assert(!ACPI::io_apic_address && "Only one I/O APIC is supported");
        ACPI::io_apic_address = io->address;
        ACPI::io_apic_interrupt_base = io->global_interrupt_base;
        break;
      }
      case 2: // ISA interrupt source override.
      {
        assert(entry->length == sizeof(MADTInterruptSourceOverride));
        const MADTInterruptSourceOverride* irq = (const MADTInterruptSourceOverride*)entry;
        assert(irq->bus == 0 && irq->irq < 16);
        ACPI::isa_irqs[irq->irq] = {irq->global_interrupt, irq->flags};
        break;
      }
      case 5: // 64-bit local APIC address override.
      {
        assert(entry->length == 12);
        uint64 address;
        memcpy(&address, cursor + 4, sizeof(address));
        assert(address <= 0xFFFFFFFF);
        ACPI::local_apic_address = address;
        break;
      }
      default:
        break;
    }
    cursor += entry->length;
  }
  assert(ACPI::num_cpus && ACPI::local_apic_address && ACPI::io_apic_address);
  kprintfd("ACPI: %zu enabled CPUs, local APIC at %x, I/O APIC at %x\n",
           ACPI::num_cpus, ACPI::local_apic_address, ACPI::io_apic_address);
}

void ACPI::initialise()
{
  for (size_t i = 0; i < 16; ++i)
    isa_irqs[i] = {(uint32)i, 0};

  uint32 ebda = (size_t)*(const uint16*)physicalAddress(0x40E, 2) << 4;
  const RSDPDescriptor* rsdp = nullptr;
  if (ebda >= 0x80000 && ebda < 0xA0000)
    rsdp = checkForRSDP(ebda, ebda + 1024);
  if (!rsdp)
    rsdp = checkForRSDP(0xE0000, 0x100000);
  assert(rsdp && "ACPI RSDP not found");
  assert(rsdp->RsdtAddress && "An ACPI RSDT is required");

  const ACPISDTHeader* rsdt = tableAt(rsdp->RsdtAddress);
  assert(memcmp(rsdt->Signature, "RSDT", 4) == 0);
  assert((rsdt->Length - sizeof(*rsdt)) % sizeof(uint32) == 0);
  const uint32* tables = (const uint32*)(rsdt + 1);
  size_t count = (rsdt->Length - sizeof(*rsdt)) / sizeof(uint32);
  for (size_t i = 0; i < count; ++i)
  {
    const ACPISDTHeader* table = tableAt(tables[i]);
    if (memcmp(table->Signature, "APIC", 4) == 0)
    {
      parseMADT((const ACPI_MADTHeader*)table);
      return;
    }
  }
  assert(false && "ACPI MADT not found");
}
