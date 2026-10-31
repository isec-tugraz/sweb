#include "stdarg.h"
#include "kprintf.h"
#include "Console.h"
#include "Terminal.h"
#include "debug_bochs.h"
#include "ArchInterrupts.h"
#include "RingBuffer.h"
#include "Scheduler.h"
#include "assert.h"
#include "debug.h"
#include "ustringformat.h"

//it's more important to keep the messages that led to an error, instead of
//the ones following it, when the nosleep buffer gets full

RingBuffer<char> *nosleep_rb_;
Thread *flush_thread_;

void flushActiveConsole()
{
  assert(main_console);
  assert(nosleep_rb_);
  assert(ArchInterrupts::testIFSet());
  char c = 0;
  while (nosleep_rb_->get(c))
  {
    main_console->getActiveTerminal()->write(c);
  }
  Scheduler::instance()->yield();
}

class KprintfFlushingThread : public Thread
{
  public:

    KprintfFlushingThread() : Thread(0, "KprintfFlushingThread", Thread::KERNEL_THREAD)
    {
    }

    virtual void Run()
    {
      while (true)
      {
        flushActiveConsole();
      }
    }
};

void kprintf_init()
{
  nosleep_rb_ = new RingBuffer<char>(1024);
  debug(KPRINTF, "Adding Important kprintf Flush Thread\n");
  flush_thread_ = new KprintfFlushingThread();
  Scheduler::instance()->addNewThread(flush_thread_);
}

void kprintf_func(int ch, void *arg __attribute__((unused)))
{
  // Interrupt handlers defer output instead of attempting to take the terminal's mutexes.
  if (system_state != RUNNING || (ArchInterrupts::testIFSet() && Scheduler::instance()->isSchedulingEnabled()))
  {
    main_console->getActiveTerminal()->write(ch);
  }
  else
  {
    nosleep_rb_->put(ch);
  }
}

void kprintf(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  kvprintf(fmt, kprintf_func, 0, 10, args);
  va_end(args);
}

void kprintfd_func(int ch, void *arg __attribute__((unused)))
{
  writeChar2Bochs((uint8) ch);
}

void kprintfd(const char *fmt, ...)
{
  static IRQSpinLock output_lock;
  // Panic diagnostics run after stopping the other CPUs, including a possible lock holder.
  bool locked = system_state == RUNNING;
  bool interrupts_enabled = locked ? output_lock.acquire() : false;
  va_list args;

  va_start(args, fmt);
  kvprintf(fmt, kprintfd_func, 0, 10, args);
  va_end(args);
  if (locked)
    output_lock.release(interrupts_enabled);
}
