#pragma once

#include "types.h"

struct RSDPDescriptor
{
  char Signature[8];
  uint8 Checksum;
  char OEMID[6];
  uint8 Revision;
  uint32 RsdtAddress;
} __attribute__((packed));

struct ACPISDTHeader
{
  char Signature[4];
  uint32 Length;
  uint8 Revision;
  uint8 Checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32 OEMRevision;
  uint32 CreatorID;
  uint32 CreatorRevision;

  bool checksumValid() const;
} __attribute__((packed));

struct ACPI_MADTHeader
{
  ACPISDTHeader header;
  uint32 local_apic_address;
  uint32 flags;
} __attribute__((packed));

struct MADTEntryDescriptor
{
  uint8 type;
  uint8 length;
} __attribute__((packed));

struct MADTProcLocalAPIC
{
  MADTEntryDescriptor header;
  uint8 processor_id;
  uint8 apic_id;
  uint32 flags;
} __attribute__((packed));

struct MADT_IO_APIC
{
  MADTEntryDescriptor header;
  uint8 id;
  uint8 reserved;
  uint32 address;
  uint32 global_interrupt_base;
} __attribute__((packed));

struct MADTInterruptSourceOverride
{
  MADTEntryDescriptor header;
  uint8 bus;
  uint8 irq;
  uint32 global_interrupt;
  uint16 flags;
} __attribute__((packed));

namespace ACPI
{
  struct IRQ
  {
    uint32 global_interrupt;
    uint16 flags;
  };

  extern uint32 local_apic_address;
  extern uint32 io_apic_address;
  extern uint32 io_apic_interrupt_base;
  extern uint8 cpu_apic_ids[256];
  extern size_t num_cpus;
  extern IRQ isa_irqs[16];

  void initialise();
}
