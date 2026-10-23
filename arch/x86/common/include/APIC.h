#pragma once

#include "types.h"

extern volatile size_t outstanding_EOIs;

namespace APIC
{
  void initialise();
  void enableIRQ(uint16 irq);
  void disableIRQ(uint16 irq);
  void enableTimer();
  void disableTimer();
  void sendEOI();
  void startCPU(uint8 apic_id, uint32 entry_address);
}
