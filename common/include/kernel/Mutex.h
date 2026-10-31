#pragma once

#include "types.h"
#include "ScopeLock.h"
#include "Lock.h"
class Thread;

class Mutex: public Lock
{
  friend class Scheduler;
  friend class Condition;

public:

  Mutex(const char* name);

  Mutex(Mutex const &) = delete;
  Mutex &operator=(Mutex const&) = delete;

  /**
   * like acquire, but instead of blocking the currentThread until the Lock is free
   * acquireNonBlocking() immediately returns True or False, depending on whether the Lock
   * was acquired successfully or already held by another Thread.
     * @param called_by A pointer to the call point of this function.
     *                  Can be set in case this method is called by a wrapper function.
   */
  bool acquireNonBlocking(pointer called_by = 0);

  void acquire(pointer called_by = 0);
  void release(pointer called_by = 0);


private:

  /**
   * The basic mutex.
   * It is atomic set to 1 when acquired,
   * and set to 0 when the mutex is released.
   */
  size_t mutex_;

};

