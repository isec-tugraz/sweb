#pragma once

#include "types.h"

class Thread;

/**
 * This call represents the locks which are used to synchronize the kernel.
 */
class Lock
{
public:
  friend class Scheduler;

  Lock(const char* name);

  /**
   * Destructor of the lock.
   * Some checks are done within there, so it can be assured that no
   * threads are waiting/holding this lock when it is destroyed.
   */
  virtual ~Lock();

  /**
   * Print out the list of threads waiting on the current thread.
   * The diagnostic lock keeps the list stable while printing.
   */
  void printWaitersList();

  /**
   * Print out the list of locks which are held by a thread (time desc order).
   * The caller must keep the thread alive; the diagnostic lock protects its holding list.
   * @param thread The thread which is holding the locks.
   */
  static void printHoldingList(Thread* thread);
  static void printWaitingOn(Thread* thread);

  Thread* heldBy() const
  {
    return __atomic_load_n(&held_by_, __ATOMIC_ACQUIRE);
  }

  bool isHeldBy(Thread* thread) const
  {
    return (heldBy() == thread);
  }

  inline const char* getName() const
  {
    return name_;
  }

  inline bool hasNextOnHoldingList() const
  {
    return next_lock_on_holding_list_;
  }

protected:

  /**
   * The thread which is holding this lock at the moment.
   */
  Thread* held_by_;

  /**
   * The single chained holding list of the thread.
   * It references to the next lock which is held by the thread which is holding this lock.
   */
  Lock* next_lock_on_holding_list_;

  /**
   * The call point which used the specified lock the last time.
   */
  pointer last_accessed_at_;

  /**
   * Remove the current thread from the holding list.
   */
  void removeCurrentThreadFromWaitersList();

  inline bool threadsAreOnWaitersList() const
  {
    return waiters_list_;
  }

  /**
   * Record the owner and add this lock to its holding list.
   */
  void recordAcquisition(pointer called_by);

  /**
   * Clear the owner and remove this lock from its holding list.
   */
  void recordRelease(pointer called_by);

  /**
   * Check if a deadlock would happen in case the current thread would wait for this lock.
   * The circular check is done within this lock.
   */
  void checkForDeadLock();

  /**
   * Check if the current thread wants to wait on a lock, even if he is still waiting for
   * another one. This is bad!
   */
  void checkCurrentThreadStillWaitingOnAnotherLock();

  /**
   * Do some checks before start to wait on another lock.
   * This is done to prevent the kernel from crushing, and is only
   * useful for the kernel programmer.
   */
  void doChecksBeforeWaiting();

  /**
   * Verifies that interrupts are enabled.
   */
  void checkInterrupts(const char* method);

  /**
   * Verifies that the lock is held by this thread.
   * @param method in which the check is done
   */
  void checkInvalidRelease(const char* method);

  /**
   * Lock the waiters list, so it may be modified.
   * The lock may not be held in case the list is read out in some special cases.
   */
  void lockWaitersList();

  /**
   * unlock the waiters list.
   */
  void unlockWaitersList();

  /**
   * Check if the waiters list is locked at the moment.
   * Only use this method for assertions, it is not ensured that the lock is held by the current thread.
   * @return true in case the lock is held by a thread at the moment
   * @return false in case the lock is not held by a thread at the moment
   */
  inline bool waitersListIsLocked() const
  {
    return waiters_list_lock_;
  }

  /**
   * Pop the longest waiting thread from the waiters list.
   * @return The thread in case one is waiting
   * @return 0 in case no thread is waiting for this lock.
   */
  Thread* popBackThreadFromWaitersList();

  /**
   * Add the current thread to the waiters list of this lock.
   */
  void pushFrontCurrentThreadToWaitersList();

  /**
   * Print the lock status.
   */
  void printStatus();

  /**
   * Release the lock, and set the thread to sleeping.
   */
  void sleepAndRelease();

private:

  /**
   * The name of the lock is a char pointer instead of a string,
   * because a string may need dynamic memory, and dynamic memory needs locks.
   */
  const char* name_;

  /**
   * The single chained list of threads waiting on this lock.
   * The diagnostic lock protects traversal against insertions and removals.
   */
  Thread* waiters_list_;

  /**
   * Serializes waiting and waking. List modifications additionally take the diagnostic lock.
   */
  size_t waiters_list_lock_;

  /**
   * Check if a deadlock would happen in combination with other locks.
   * @param thread_waiting The thread which wants to wait on the lock
   * @param start The lock which the thread wants to wait on
   */
  void checkForCircularDeadLock(Thread* thread_waiting, Lock* start);

  /**
   * Print out a circular deadlock hierarchy.
   * @param starting The thread which shall be started at
   */
  void printOutCircularDeadLock(Thread* starting);

};

