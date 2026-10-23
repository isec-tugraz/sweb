#include <types.h>
#include <Thread.h>
#include <Scheduler.h>
#include <ArchInterrupts.h>
#include "kprintf.h"
#include "panic.h"
#include "debug_bochs.h"
#ifdef CMAKE_X86_64
#include "ArchMulticore.h"
#endif

void kpanict ( const char * message )
{
  ArchInterrupts::disableInterrupts();
#ifdef CMAKE_X86_64
  ArchMulticore::stopOtherCPUs();
#endif
  system_state = KPANIC;

  size_t* stack = currentThread ? (size_t*)currentThread->getKernelStackStartPointer() : nullptr;

  kprintfd("%s \n", message );
  kprintf("%s \n", message );

  kprintfd( "KPANICT: stack is > %p ",  stack );
  //kprintf( "KPANICT: stack is > %x ",  stack );

  if (currentThread)
    currentThread->printBacktrace(false);

  Scheduler::instance()->printThreadList();

  ArchInterrupts::disableInterrupts();
  ArchInterrupts::disableTimer();
  //disable other IRQ's ???

  for(;;);

  kprintf("MAJOR KERNEL PANIC!: Should never reach here\n");

  currentThread->printBacktrace(false);

  Scheduler::instance()->printThreadList();

}
