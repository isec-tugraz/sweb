#include "interrupt_registers.h"
#include "panic.h"

void arch_saveThreadRegisters(size_t* frame, size_t error_code);
size_t arch_computeDummyHandler(size_t instruction_pointer);
void errorHandler(size_t number, size_t instruction_pointer, size_t code_segment, size_t spurious);
void pageFaultHandler(size_t address, size_t error_code, size_t instruction_pointer);
void syscallHandler(void);

#define PUSH_ALL            \
  "pushal\n"               \
  "push %ds\n"             \
  "push %es\n"             \
  "movw $0x10, %ax\n" /* Kernel data-segment selector. */ \
  "movw %ax, %es\n"        \
  "movw %ax, %ds\n"

#define POP_ALL             \
  "pop %es\n"              \
  "pop %ds\n"              \
  "popal\n"

static __attribute__((noinline)) void saveThreadRegisters(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved EBP and the return address.
  arch_saveThreadRegisters(function_frame, 0);
}

static __attribute__((noinline)) void saveThreadRegistersWithErrorCode(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved EBP and the return address.
  arch_saveThreadRegisters(function_frame, 1);
}

#define DEFINE_IRQ_HANDLER(number)                              \
  void irqHandler_##number(void);                               \
                                                               \
  __attribute__((naked)) void arch_irqHandler_##number(void)    \
  {                                                            \
    asm volatile(PUSH_ALL);                                    \
    saveThreadRegisters();                                     \
    irqHandler_##number();                                     \
    asm volatile(POP_ALL "iretl\n");                          \
  }

#define DEFINE_ERROR_HANDLER(number, error_code)                 \
  static __attribute__((noinline)) void errorEntry_##number(void) \
  {                                                              \
    size_t* function_frame = __builtin_frame_address(0);          \
    function_frame += 2; /* Skip saved EBP and the return address. */ \
    struct context_switch_registers* saved = (struct context_switch_registers*)function_frame; \
    struct interrupt_registers* interrupt =                       \
        (struct interrupt_registers*)((size_t*)(saved + 1) + error_code); \
    errorHandler(number, interrupt->eip, interrupt->cs, 0);       \
  }                                                              \
                                                                 \
  __attribute__((naked)) void arch_errorHandler_##number(void)   \
  {                                                              \
    asm volatile(PUSH_ALL);                                      \
    if (error_code)                                               \
      saveThreadRegistersWithErrorCode();                         \
    else                                                          \
      saveThreadRegisters();                                      \
    errorEntry_##number();                                       \
    asm volatile(POP_ALL);                                       \
    if (error_code)                                               \
      asm volatile("addl $4, %esp\n"); /* Discard the CPU-pushed error code. */ \
    asm volatile("iretl\n");                                   \
  }

static __attribute__((noinline)) void dummyEntry(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved EBP and the return address.
  struct context_switch_registers* saved = (struct context_switch_registers*)function_frame;
  struct dummy_interrupt_registers* dummy = (struct dummy_interrupt_registers*)(saved + 1);
  size_t interrupt = arch_computeDummyHandler(dummy->return_address);
  errorHandler(interrupt, dummy->interrupt.eip, dummy->interrupt.cs, 1);
}

__attribute__((naked, aligned(1))) void arch_dummyHandler(void)
{
  asm volatile(".rept 128\n"
               "call arch_dummyHandlerMiddle\n" // Encode the vector in the return address.
               ".endr\n"
               ".global arch_dummyHandlerMiddle\n"
               "arch_dummyHandlerMiddle:\n"
               PUSH_ALL);
  saveThreadRegisters();
  dummyEntry();
  asm volatile(POP_ALL
               "addl $4, %esp\n" // Discard the synthetic return address.
               "iretl\n");
}

static __attribute__((noinline)) void pageFaultEntry(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved EBP and the return address.
  struct context_switch_registers* saved = (struct context_switch_registers*)function_frame;
  struct exception_registers* exception = (struct exception_registers*)(saved + 1);
  size_t address;
  asm volatile("movl %%cr2, %0" : "=r"(address)); // CR2 contains the faulting address.
  pageFaultHandler(address, exception->error_code, exception->interrupt.eip);
}

__attribute__((naked)) void arch_pageFaultHandler(void)
{
  asm volatile(PUSH_ALL);
  saveThreadRegistersWithErrorCode();
  pageFaultEntry();
  asm volatile(POP_ALL
               "addl $4, %esp\n" // Discard the CPU-pushed page-fault error code.
               "iretl\n");
}

DEFINE_IRQ_HANDLER(0)
DEFINE_IRQ_HANDLER(1)
DEFINE_IRQ_HANDLER(3)
DEFINE_IRQ_HANDLER(4)
DEFINE_IRQ_HANDLER(6)
DEFINE_IRQ_HANDLER(9)
DEFINE_IRQ_HANDLER(11)
DEFINE_IRQ_HANDLER(14)
DEFINE_IRQ_HANDLER(15)
DEFINE_IRQ_HANDLER(65)

DEFINE_ERROR_HANDLER(8, 1)
DEFINE_ERROR_HANDLER(10, 1)
DEFINE_ERROR_HANDLER(11, 1)
DEFINE_ERROR_HANDLER(12, 1)
DEFINE_ERROR_HANDLER(13, 1)
DEFINE_ERROR_HANDLER(14, 1)
DEFINE_ERROR_HANDLER(17, 1)

DEFINE_ERROR_HANDLER(0, 0)
DEFINE_ERROR_HANDLER(4, 0)
DEFINE_ERROR_HANDLER(5, 0)
DEFINE_ERROR_HANDLER(6, 0)
DEFINE_ERROR_HANDLER(7, 0)
DEFINE_ERROR_HANDLER(9, 0)
DEFINE_ERROR_HANDLER(16, 0)
DEFINE_ERROR_HANDLER(18, 0)
DEFINE_ERROR_HANDLER(19, 0)

__attribute__((naked)) void arch_syscallHandler(void)
{
  asm volatile(PUSH_ALL);
  saveThreadRegisters();
  syscallHandler();
  kpanict("syscallHandler returned");
  asm volatile("hlt\n");
}
