#include "ArchInterrupts.h"
#include "Scheduler.h"
#include "kprintf.h"
#include "KernelMemoryManager.h"
#include "Thread.h"
#include "types.h"
#ifdef CMAKE_X86_64
#include "ArchMulticore.h"
#endif

namespace ustl
{
  void checkKMMDeadlock()
  {
    bool interrupts_enabled = ArchInterrupts::testIFSet();
    if (unlikely (interrupts_enabled == false || Scheduler::instance()->isSchedulingEnabled() == false))
    {
      if (unlikely (KernelMemoryManager::instance()->KMMLockHeldBy() != 0))
      {
#ifdef CMAKE_X86_64
        ArchMulticore::stopOtherCPUs();
#endif
        system_state = KPANIC;
        kprintfd("(ERROR) checkKMMDeadlock: Using a not resize-safe ustl container method with IF=%d and SchedulingEnabled=%d ! This will fail!!!\n",
                 interrupts_enabled, Scheduler::instance()->isSchedulingEnabled());
        currentThread->printBacktrace(true);
        assert(false);
      }
    }
  }
}
