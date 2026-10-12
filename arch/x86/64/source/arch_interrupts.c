#include "interrupt_registers.h"
#include "panic.h"

void arch_saveThreadRegisters(size_t* frame, size_t error_code);
void errorHandler(size_t number, size_t instruction_pointer, size_t code_segment, size_t spurious);
void pageFaultHandler(size_t address, size_t error_code, size_t instruction_pointer);
void syscallHandler(void);

#define PUSH_ALL                         \
  "pushq %rsp\n"                       \
  "pushq %rax\n"                       \
  "pushq %rcx\n"                       \
  "pushq %rdx\n"                       \
  "pushq %rbx\n"                       \
  "pushq %rbp\n"                       \
  "pushq %rsi\n"                       \
  "pushq %rdi\n"                       \
  "pushq %r8\n"                        \
  "pushq %r9\n"                        \
  "pushq %r10\n"                       \
  "pushq %r11\n"                       \
  "pushq %r12\n"                       \
  "pushq %r13\n"                       \
  "pushq %r14\n"                       \
  "pushq %r15\n"                       \
  "movw %es, %ax\n"                    \
  "pushq %rax\n"                       \
  "movw %ds, %ax\n"                    \
  "pushq %rax\n"                       \
  "movw $0x20, %ax\n" /* Kernel data-segment selector. */ \
  "movw %ax, %ss\n"                    \
  "movw %ax, %ds\n"                    \
  "movw %ax, %es\n"                    \
  "movw %ax, %fs\n"                    \
  "movw %ax, %gs\n"

#define POP_ALL                          \
  "popq %rax\n"                        \
  "movw %ax, %ds\n"                    \
  "popq %rax\n"                        \
  "movw %ax, %es\n"                    \
  "popq %r15\n"                        \
  "popq %r14\n"                        \
  "popq %r13\n"                        \
  "popq %r12\n"                        \
  "popq %r11\n"                        \
  "popq %r10\n"                        \
  "popq %r9\n"                         \
  "popq %r8\n"                         \
  "popq %rdi\n"                        \
  "popq %rsi\n"                        \
  "popq %rbp\n"                        \
  "popq %rbx\n"                        \
  "popq %rdx\n"                        \
  "popq %rcx\n"                        \
  "popq %rax\n"                        \
  "popq %rsp\n"

static __attribute__((noinline)) void saveThreadRegisters(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved RBP and the return address.
  arch_saveThreadRegisters(function_frame, 0);
}

static __attribute__((noinline)) void saveThreadRegistersWithErrorCode(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved RBP and the return address.
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
    asm volatile(POP_ALL "iretq\n");                          \
  }

#define DEFINE_ERROR_HANDLER(number, error_code)                 \
  static __attribute__((noinline)) void errorEntry_##number(void) \
  {                                                              \
    size_t* function_frame = __builtin_frame_address(0);          \
    function_frame += 2; /* Skip saved RBP and the return address. */ \
    struct context_switch_registers* saved = (struct context_switch_registers*)function_frame; \
    struct interrupt_registers* interrupt =                       \
        (struct interrupt_registers*)((size_t*)(saved + 1) + error_code); \
    errorHandler(number, interrupt->rip, interrupt->cs, 0);       \
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
      asm volatile("addq $8, %rsp\n"); /* Discard the CPU-pushed error code. */ \
    asm volatile("iretq\n");                                   \
  }

static size_t dummyhandlerscratchvariable;

static __attribute__((noinline)) void dummyEntry(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved RBP and the return address.
  struct context_switch_registers* saved = (struct context_switch_registers*)function_frame;
  struct interrupt_registers* interrupt = (struct interrupt_registers*)(saved + 1);
  size_t interrupt_number = dummyhandlerscratchvariable + 128;
  dummyhandlerscratchvariable = 0;
  errorHandler(interrupt_number, interrupt->rip, interrupt->cs, 1);
}

__attribute__((naked, aligned(1))) void arch_dummyHandler(void)
{
  asm volatile(".rept 128\n"
               "decq dummyhandlerscratchvariable\n" // Encode the vector in the entry offset.
               ".endr\n"
               ".global arch_dummyHandlerMiddle\n"
               "arch_dummyHandlerMiddle:\n"
               PUSH_ALL);
  saveThreadRegisters();
  dummyEntry();
  asm volatile(POP_ALL "iretq\n");
}

static __attribute__((noinline)) void pageFaultEntry(void)
{
  size_t* function_frame = __builtin_frame_address(0);
  function_frame += 2; // Skip saved RBP and the return address.
  struct context_switch_registers* saved = (struct context_switch_registers*)function_frame;
  struct exception_registers* exception = (struct exception_registers*)(saved + 1);
  size_t address;
  asm volatile("movq %%cr2, %0" : "=r"(address)); // CR2 contains the faulting address.
  pageFaultHandler(address, exception->error_code, exception->interrupt.rip);
}

__attribute__((naked)) void arch_pageFaultHandler(void)
{
  asm volatile(PUSH_ALL);
  saveThreadRegistersWithErrorCode();
  pageFaultEntry();
  asm volatile(POP_ALL
               "addq $8, %rsp\n" // Discard the CPU-pushed page-fault error code.
               "iretq\n");
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
