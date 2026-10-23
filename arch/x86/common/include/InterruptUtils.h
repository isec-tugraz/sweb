#pragma once

#include "types.h"
#include "cpu_local.h"

typedef struct {
  uint32  number;      // handler number
  void (*offset)();    // pointer to handler function
}  __attribute__((__packed__)) InterruptHandlers;


typedef struct {
    uint16 limit;
    size_t base;
} __attribute__((__packed__)) IDTR ;


class InterruptUtils
{
public:
  static void initialise();

  static void lidt(IDTR *idtr);

  static void countPageFault(uint64 address);

private:
  static InterruptHandlers handlers[];
  static __cpu uint64 pf_address;
  static __cpu uint64 pf_address_counter;
};

