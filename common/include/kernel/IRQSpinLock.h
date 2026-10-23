#pragma once

#include "ArchInterrupts.h"
#include "ArchThreads.h"

// For short critical sections shared with interrupt handlers. Never sleep or allocate while holding this lock.
class IRQSpinLock
{
public:
  constexpr IRQSpinLock() : locked_(0) {}
  IRQSpinLock(const IRQSpinLock&) = delete;
  IRQSpinLock& operator=(const IRQSpinLock&) = delete;

  bool acquire()
  {
    bool interrupts_enabled = ArchInterrupts::disableInterrupts();
    while (ArchThreads::testSetLock(locked_, 1));
    return interrupts_enabled;
  }

  void release(bool interrupts_enabled)
  {
    ArchThreads::atomic_set(locked_, 0);
    if (interrupts_enabled)
      ArchInterrupts::enableInterrupts();
  }

private:
  size_t locked_;
};

class ScopeIRQSpinLock
{
public:
  explicit ScopeIRQSpinLock(IRQSpinLock& lock) : lock_(lock), interrupts_enabled_(lock.acquire()) {}
  ~ScopeIRQSpinLock() { lock_.release(interrupts_enabled_); }
  ScopeIRQSpinLock(const ScopeIRQSpinLock&) = delete;
  ScopeIRQSpinLock& operator=(const ScopeIRQSpinLock&) = delete;

private:
  IRQSpinLock& lock_;
  bool interrupts_enabled_;
};
