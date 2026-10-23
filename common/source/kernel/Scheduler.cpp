#include "Scheduler.h"
#include "Thread.h"
#include "panic.h"
#include "ArchThreads.h"
#include "ArchCommon.h"
#include "kprintf.h"
#include "ArchInterrupts.h"
#include "KernelMemoryManager.h"
#include <ulist.h>
#include "backtrace.h"
#include "ArchThreads.h"
#include "Mutex.h"
#include "umap.h"
#include "ustring.h"
#include "Lock.h"

__cpu ArchThreadRegisters *currentThreadRegisters;
__cpu Thread *currentThread;

__cpu Scheduler Scheduler::scheduler_;

Scheduler *Scheduler::instance()
{
  return &scheduler_;
}

Scheduler::Scheduler() : incoming_lock_("Scheduler::incoming_lock_")
{
  block_scheduling_ = 0;
  ticks_ = 0;
  unfinished_cleanup_counter_ = 0;
  addNewThread(&cleanup_thread_);
  addNewThread(&idle_thread_);
}

void Scheduler::schedule()
{
  assert(this == instance());
  assert(!ArchInterrupts::testIFSet() && "Tried to schedule with Interrupts enabled");
  if (block_scheduling_)
  {
    debug(SCHEDULER, "schedule: currently blocked\n");
    return;
  }

  if (currentThread == &cleanup_thread_ && unfinished_cleanup_counter_ > 5)
  {
    debug(SCHEDULER, "schedule: WARNING - cleanup_thread is being descheduled before completing cleanup.\n");
  }

  auto it = threads_.begin();
  for(; it != threads_.end(); ++it)
  {
    assert((*it)->scheduler_ == this);
    if((*it)->schedulable())
    {
      currentThread = *it;
      break;
    }
  }

  assert(it != threads_.end() && "No schedulable thread found");
  ustl::rotate(threads_.begin(), it + 1, threads_.end()); // no new/delete here - important because interrupts are disabled
  //debug(SCHEDULER, "Scheduler::schedule: new currentThread is %p %s, switch_to_userspace: %d\n", currentThread, currentThread->getName(), currentThread->switch_to_userspace_);
  currentThreadRegisters = currentThread->switch_to_userspace_ ? currentThread->user_registers_ : currentThread->kernel_registers_;
}

void Scheduler::addNewThread(Thread *thread)
{
  assert(thread);
  debug(SCHEDULER, "addNewThread: %p  %zd:%s\n", thread, thread->getTID(), thread->getName());
  if (currentThread)
    ArchThreads::debugCheckNewThread(thread);
  if (this != instance())
  {
    ScopeLock lock(incoming_lock_);
    assert(!thread->scheduler_ && "Thread already belongs to a scheduler");
    thread->scheduler_ = this;
    incoming_threads_.push_back(thread);
    return;
  }
  KernelMemoryManager::instance()->getKMMLock().acquire();
  lockScheduling();
  KernelMemoryManager::instance()->getKMMLock().release();
  assert(!thread->scheduler_ && "Thread already belongs to a scheduler");
  thread->scheduler_ = this;
  threads_.push_back(thread);
  unlockScheduling();
}

void Scheduler::receiveThreads()
{
  assert(this == instance());
  ScopeLock lock(incoming_lock_);
  if (incoming_threads_.empty())
    return;
  KernelMemoryManager::instance()->getKMMLock().acquire();
  lockScheduling();
  KernelMemoryManager::instance()->getKMMLock().release();
  for (Thread* thread : incoming_threads_)
  {
    assert(thread->scheduler_ == this);
    threads_.push_back(thread);
  }
  incoming_threads_.clear();
  unlockScheduling();
}

void Scheduler::sleep(Lock* waiters_lock)
{
  assert(this == instance() && currentThread->scheduler_ == this);
  assert(block_scheduling_ == 0);
  // A timer must not switch threads between marking us sleeping and releasing the waiters list.
  bool interrupts_enabled = ArchInterrupts::disableInterrupts();
  currentThread->setState(Sleeping);
  if (waiters_lock)
    waiters_lock->unlockWaitersList();
  ArchThreads::yield();
  if (interrupts_enabled)
    ArchInterrupts::enableInterrupts();
}

void Scheduler::wake(Thread* thread_to_wake)
{
  // The thread stays in its owner's list; another CPU may mark it runnable here.
  assert(thread_to_wake->scheduler_);
  assert(thread_to_wake->getState() == Sleeping && "Thread must be sleeping before it can be woken");
  thread_to_wake->setState(Running);
}

void Scheduler::yield()
{
  assert(this);
  if (!ArchInterrupts::testIFSet())
  {
    assert(currentThread);
    kprintfd("Scheduler::yield: WARNING Interrupts disabled, do you really want to yield ? (currentThread %p %s)\n",
             currentThread, currentThread->name_.c_str());
    currentThread->printBacktrace();
  }
  ArchThreads::yield();
}

void Scheduler::cleanupDeadThreads()
{
  /* Before adding new functionality to this function, consider if that
     functionality could be implemented more cleanly in another place.
     (e.g. Thread/Process destructor) */

  assert(currentThread == &cleanup_thread_);

  lockScheduling();
  uint32 thread_count_max = sizeof(cleanup_thread_.kernel_stack_) / (2 * sizeof(Thread*));
  thread_count_max = ustl::min(thread_count_max, threads_.size());
  Thread* destroy_list[thread_count_max];
  uint32 thread_count = 0;
  for (uint32 i = 0; i < threads_.size() && thread_count < thread_count_max; ++i)
  {
    Thread* tmp = threads_[i];
    assert(tmp->scheduler_ == this);
    if (tmp->getState() == ToBeDestroyed)
    {
      // Only this CPU can run or clean up its threads, so the old stack is no longer in use.
      assert(tmp != currentThread && !tmp->holding_lock_list_ && !tmp->lock_waiting_on_);
      destroy_list[thread_count++] = tmp;
      threads_.erase(threads_.begin() + i); // Note: erase will not realloc!
      --i;
    }
  }
  unlockScheduling();
  if (thread_count > 0)
  {
    ArchThreads::atomic_set(unfinished_cleanup_counter_, thread_count);
    for (uint32 i = 0; i < thread_count; ++i)
    {
      delete destroy_list[i];
      ArchThreads::atomic_add(unfinished_cleanup_counter_, -1);
    }
    debug(SCHEDULER, "cleanupDeadThreads: done\n");
  }
}

void Scheduler::printThreadList()
{
  lockScheduling();
  debug(SCHEDULER, "Scheduler::printThreadList: %zd Threads in List\n", threads_.size());
  for (size_t c = 0; c < threads_.size(); ++c)
    debug(SCHEDULER, "Scheduler::printThreadList: threads_[%zd]: %p  %zd:%25s     [%s]\n", c, threads_[c],
          threads_[c]->getTID(), threads_[c]->getName(), Thread::threadStatePrintable[threads_[c]->getState()]);
  unlockScheduling();
}

void Scheduler::lockScheduling() //not as severe as stopping Interrupts
{
  assert(this == instance() && "Scheduler lists may only be changed on their owning CPU");
  if (unlikely(ArchThreads::testSetLock(block_scheduling_, 1)))
    kpanict("FATAL ERROR: Scheduler::*: block_scheduling_ was set !! How the Hell did the program flow get here then ?\n");
}

void Scheduler::unlockScheduling()
{
  block_scheduling_ = 0;
}

bool Scheduler::isSchedulingEnabled()
{
  return this && block_scheduling_ == 0;
}

bool Scheduler::isCurrentlyCleaningUp()
{
  return currentThread == &cleanup_thread_;
}

size_t Scheduler::getTicks()
{
  return ticks_;
}

void Scheduler::incTicks()
{
  ++ticks_;
}

void Scheduler::printStackTraces()
{
  lockScheduling();
  debug(BACKTRACE, "printing the backtraces of <%zd> threads:\n", threads_.size());

  for (const auto& thread : threads_)
  {
    thread->printBacktrace();
    debug(BACKTRACE, "\n");
    debug(BACKTRACE, "\n");
  }

  unlockScheduling();
}

void Scheduler::printLockingInformation()
{
  lockScheduling();
  kprintfd("\n");
  debug(LOCK, "Scheduler::printLockingInformation:\n");

  for(Thread* t : threads_)
  {
    if(t->holding_lock_list_)
      Lock::printHoldingList(t);
  }
  for(Thread* t : threads_)
  {
    Lock::printWaitingOn(t);
  }
  debug(LOCK, "Scheduler::printLockingInformation finished\n");
  unlockScheduling();
}
