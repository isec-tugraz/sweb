#pragma once

#include "types.h"

struct context_switch_registers
{
  size_t ds;
  size_t es;
  size_t r15;
  size_t r14;
  size_t r13;
  size_t r12;
  size_t r11;
  size_t r10;
  size_t r9;
  size_t r8;
  size_t rdi;
  size_t rsi;
  size_t rbp;
  size_t rbx;
  size_t rdx;
  size_t rcx;
  size_t rax;
  size_t rsp;
};

struct interrupt_registers
{
  size_t rip;
  size_t cs;
  size_t rflags;
  size_t rsp;
  size_t ss;
};

struct exception_registers
{
  size_t error_code;
  struct interrupt_registers interrupt;
};

#ifdef __cplusplus
static_assert(sizeof(struct context_switch_registers) == 18 * sizeof(size_t));
static_assert(sizeof(struct interrupt_registers) == 5 * sizeof(size_t));
static_assert(sizeof(struct exception_registers) == 6 * sizeof(size_t));
#else
_Static_assert(sizeof(struct context_switch_registers) == 18 * sizeof(size_t), "");
_Static_assert(sizeof(struct interrupt_registers) == 5 * sizeof(size_t), "");
_Static_assert(sizeof(struct exception_registers) == 6 * sizeof(size_t), "");
#endif
