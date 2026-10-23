#pragma once

#include "ArchThreads.h"

struct CPU
{
  struct TSS
  {
    uint32 reserved_0;
    size_t rsp0;
    size_t rsp1;
    size_t rsp2;
    uint64 reserved_1;
    size_t ist[7];
    uint64 reserved_2;
    uint16 reserved_3;
    uint16 iomap;
  } __attribute__((packed));

  uint8 apic_id;
  bool online;
  void* tls;
  uint8* stack;
  ArchThreadRegisters registers;
  SegmentDescriptor gdt[7];
  TSS tss;
};

static_assert(sizeof(CPU::TSS) == 104);
extern __cpu CPU* currentCPU;

namespace ArchMulticore
{
  void initialise();
}
