#pragma once

#include "types.h"

struct context_switch_registers
{
  size_t es;
  size_t ds;
  size_t edi;
  size_t esi;
  size_t ebp;
  size_t esp;
  size_t ebx;
  size_t edx;
  size_t ecx;
  size_t eax;
};

struct interrupt_registers
{
  size_t eip;
  size_t cs;
  size_t eflags;
  size_t esp3;
  size_t ss3;
};

struct exception_registers
{
  size_t error_code;
  struct interrupt_registers interrupt;
};

struct dummy_interrupt_registers
{
  size_t return_address;
  struct interrupt_registers interrupt;
};

#ifdef __cplusplus
static_assert(sizeof(struct context_switch_registers) == 10 * sizeof(size_t));
static_assert(sizeof(struct interrupt_registers) == 5 * sizeof(size_t));
static_assert(sizeof(struct exception_registers) == 6 * sizeof(size_t));
static_assert(sizeof(struct dummy_interrupt_registers) == 6 * sizeof(size_t));
#else
_Static_assert(sizeof(struct context_switch_registers) == 10 * sizeof(size_t), "");
_Static_assert(sizeof(struct interrupt_registers) == 5 * sizeof(size_t), "");
_Static_assert(sizeof(struct exception_registers) == 6 * sizeof(size_t), "");
_Static_assert(sizeof(struct dummy_interrupt_registers) == 6 * sizeof(size_t), "");
#endif
