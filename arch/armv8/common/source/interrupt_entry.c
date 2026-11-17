#include "types.h"
#include "ArchThreadRegisters.h"
#include "pseudo_naked.h"

#pragma GCC diagnostic ignored "-Wattributes"

// Parts of this code are from ARM's "Bare-metal Boot Code for ARMv8-A Processors":
// http://infocenter.arm.com/help/topic/com.arm.doc.dai0527a/DAI0527A_baremetal_boot_code_for_ARMv8_A_processors.pdf

void interruptEntry(size_t interrupt_id, size_t current_el, size_t exception_syndrome,
                    size_t fault_address, size_t return_address);

static __attribute__((noinline)) void saveThreadRegisters(
    ArchThreadRegisters* registers, size_t spsr, size_t return_address, size_t stack_pointer, size_t ttbr0)
{
  registers->SPSR = spsr;
  registers->ELR = return_address;
  registers->SP = stack_pointer;
  registers->TTBR0 = ttbr0;
}

static __attribute__((noinline)) void dispatchException(size_t interrupt_id, size_t return_address)
{
  size_t current_el;
  size_t exception_syndrome;
  size_t fault_address;
  asm volatile(
      "mrs %0, CurrentEL\n" // Exception level at which the handler runs.
      "mrs %1, ESR_EL1\n"   // Exception class and syndrome information.
      "mrs %2, FAR_EL1\n"   // Address associated with a synchronous fault.
      : "=r"(current_el), "=r"(exception_syndrome), "=r"(fault_address));

  interruptEntry(interrupt_id, current_el, exception_syndrome, fault_address, return_address);
}

static __attribute__((noinline)) void exceptionEntry(
    size_t interrupt_id, ArchThreadRegisters* registers, size_t spsr,
    size_t return_address, size_t stack_pointer, size_t ttbr0)
{
  saveThreadRegisters(registers, spsr, return_address, stack_pointer, ttbr0);
  dispatchException(interrupt_id, return_address);
}

__attribute__((naked, noinline, noreturn)) void arm_exception_entry(void)
{
  register size_t interrupt_id asm("x0");
  register ArchThreadRegisters* registers asm("x1");
  register size_t spsr asm("x2");
  register size_t return_address asm("x3");
  register size_t stack_pointer asm("x4");
  register size_t ttbr0 asm("x5");

  PSEUDO_NAKED_BODY(arm_exception_entry);

  asm volatile(
    ".extern currentThreadRegisters\n"
    ".extern boot_stack\n"
    "\n"
    "    SUB X1  , X2   , #%c[q_offset]\n" // Recover the register-set base from &Q[0].
    "    STP Q0  , Q1   , [X2, #32 * 0]\n" // Save all FP/SIMD registers.
    "    STP Q2  , Q3   , [X2, #32 * 1]\n"
    "    STP Q4  , Q5   , [X2, #32 * 2]\n"
    "    STP Q6  , Q7   , [X2, #32 * 3]\n"
    "    STP Q8  , Q9   , [X2, #32 * 4]\n"
    "    STP Q10 , Q11  , [X2, #32 * 5]\n"
    "    STP Q12 , Q13  , [X2, #32 * 6]\n"
    "    STP Q14 , Q15  , [X2, #32 * 7]\n"
    "    STP Q16 , Q17  , [X2, #32 * 8]\n"
    "    STP Q18 , Q19  , [X2, #32 * 9]\n"
    "    STP Q20 , Q21  , [X2, #32 * 10]\n"
    "    STP Q22 , Q23  , [X2, #32 * 11]\n"
    "    STP Q24 , Q25  , [X2, #32 * 12]\n"
    "    STP Q26 , Q27  , [X2, #32 * 13]\n"
    "    STP Q28 , Q29  , [X2, #32 * 14]\n"
    "    STP Q30 , Q31  , [X2, #32 * 15]\n"
    "\n"
    "    MRS X2 , SPSR_EL1\n" // Saved processor state of the interrupted context.
    "    MRS X3 , ELR_EL1\n"  // Address at which the interrupted context resumes.
    "    MRS X5 , TTBR0_EL1\n" // Lower virtual address translation table.
    "    LDR X8 , [X1, #%c[kernel_sp_offset]]\n" // Kernel stack for exceptions from EL0.
    "\n"
    "    AND X7 , X2, #0xf\n" // SPSR.M is zero for an exception from EL0.
    "    CBNZ X7, .Lexception_from_kernel\n"
    "    MRS X4 , SP_EL0\n"   // Preserve the interrupted user stack pointer.
    "    MOV SP , X8\n"       // Dispatch on the current thread's kernel stack.
    "    B .Ldispatch_exception\n"
    ".Lexception_from_kernel:\n"
    "    MOV X4 , SP\n"       // Preserve the interrupted kernel stack pointer.
    ".Ldispatch_exception:\n"
    ".global interrupt_dispatch_start\n"
    "interrupt_dispatch_start:\n"
    : "=r"(interrupt_id), "=r"(registers), "=r"(spsr),
      "=r"(return_address), "=r"(stack_pointer), "=r"(ttbr0)
    : [q_offset] "i"(__builtin_offsetof(ArchThreadRegisters, Q)),
      [kernel_sp_offset] "i"(__builtin_offsetof(ArchThreadRegisters, SP_SM))
    : "memory"
  );

  exceptionEntry(interrupt_id, registers, spsr, return_address, stack_pointer, ttbr0);

  asm volatile(
    ".global interrupt_dispatch_end\n"
    "interrupt_dispatch_end:\n"
    "\n"
    "restore_context:\n"
    "    LDR  X0 , =currentThreadRegisters\n" // The scheduler may have selected another thread.
    "    LDR  X0 , [X0]\n"
    "    LDR X1 , [X0, #%c[sp_offset]]\n"
    "    LDR X2 , [X0, #%c[ttbr0_offset]]\n"
    "    LDR X3 , [X0, #%c[spsr_offset]]\n"
    "    LDR X4 , [X0, #%c[elr_offset]]\n"
    "\n"
    "check_sp:\n"
    "    AND X6 , X3 , #0xf\n" // SPSR.M selects the stack pointer to restore.
    "    CBNZ X6 , restore_sp_sm\n"
    "    B           restore_sp_um\n"
    "\n"
    "restore_sp_sm:\n"
    "    MOV   SP  , X1\n"
    "    B        restore_rest\n"
    "\n"
    "restore_sp_um:\n"
    "    MSR SP_EL0    , X1\n" // Restore the user stack without using it in the kernel.
    "    LDR X1 , =boot_stack\n"
    "    ADD X1 , X1 , #0x4000\n"
    "    MOV SP , X1\n"
    "\n"
    "restore_rest:\n"
    "    MSR TTBR0_EL1 , X2\n"
    "    MSR SPSR_EL1  , X3\n"
    "    MSR ELR_EL1   , X4\n"
    "\n"
    "\n"
    "    DSB ISHST\n"
    "    //TLBI ALLE1\n"
    "    DSB ISH\n"
    "    ISB\n"
    "\n"
    "\n"
    "    ADD X0 , X0 , #%c[q_end_offset]\n" // Restore Q registers backwards from &Q[32].
    "\n"
    "    LDP Q30, Q31 , [X0, #-32 * 1]\n"
    "    LDP Q28, Q29 , [X0, #-32 * 2]\n"
    "    LDP Q26, Q27 , [X0, #-32 * 3]\n"
    "    LDP Q24, Q25 , [X0, #-32 * 4]\n"
    "    LDP Q22, Q23 , [X0, #-32 * 5]\n"
    "    LDP Q20, Q21 , [X0, #-32 * 6]\n"
    "    LDP Q18, Q19 , [X0, #-32 * 7]\n"
    "    LDP Q16, Q17 , [X0, #-32 * 8]\n"
    "    LDP Q14, Q15 , [X0, #-32 * 9]\n"
    "    LDP Q12, Q13 , [X0, #-32 * 10]\n"
    "    LDP Q10, Q11 , [X0, #-32 * 11]\n"
    "    LDP Q8 , Q9  , [X0, #-32 * 12]\n"
    "    LDP Q6 , Q7  , [X0, #-32 * 13]\n"
    "    LDP Q4 , Q5  , [X0, #-32 * 14]\n"
    "    LDP Q2 , Q3  , [X0, #-32 * 15]\n"
    "    LDP Q0 , Q1  , [X0, #-32 * 16]\n"
    "\n"
    "    SUB X0 , X0 , #%c[q_size]\n" // Position X0 directly after the general registers.
    "\n"
    "    LDP X29 , X30, [X0, #-16 * 1]\n"
    "    LDP X27 , X28, [X0, #-16 * 2]\n"
    "    LDP X25 , X26, [X0, #-16 * 3]\n"
    "    LDP X23 , X24, [X0, #-16 * 4]\n"
    "    LDP X21 , X22, [X0, #-16 * 5]\n"
    "    LDP X19 , X20, [X0, #-16 * 6]\n"
    "    LDP X17 , X18, [X0, #-16 * 7]\n"
    "    LDP X15 , X16, [X0, #-16 * 8]\n"
    "    LDP X13 , X14, [X0, #-16 * 9]\n"
    "    LDP X11 , X12, [X0, #-16 * 10]\n"
    "    LDP X9  , X10, [X0, #-16 * 11]\n"
    "    LDP X7  , X8 , [X0, #-16 * 12]\n"
    "    LDP X5  , X6 , [X0, #-16 * 13]\n"
    "    LDP X3  , X4 , [X0, #-16 * 14]\n"
    "    LDP X1  , X2 , [X0, #-16 * 15]\n"
    "    LDR X0  , [X0, #(-16 * 15)-8]\n"
    "\n"
    "    dmb sy\n"
    "    dsb sy\n"
    "    isb sy\n"
    "\n"
    "    ERET\n" // Restore PSTATE from SPSR_EL1 and PC from ELR_EL1.
    :
    : [sp_offset] "i"(__builtin_offsetof(ArchThreadRegisters, SP)),
      [ttbr0_offset] "i"(__builtin_offsetof(ArchThreadRegisters, TTBR0)),
      [spsr_offset] "i"(__builtin_offsetof(ArchThreadRegisters, SPSR)),
      [elr_offset] "i"(__builtin_offsetof(ArchThreadRegisters, ELR)),
      [q_end_offset] "i"(__builtin_offsetof(ArchThreadRegisters, Q) + sizeof(((ArchThreadRegisters*)0)->Q)),
      [q_size] "i"(sizeof(((ArchThreadRegisters*)0)->Q))
    : "memory"
  );
  __builtin_unreachable();
}

__attribute__((noinline, noreturn, used)) void arm_invalid_exception(void)
{
  for (;;)
    asm volatile("wfi");
}

__attribute__((noinline, noreturn, used)) void arm_exception_vectors(void)
{
  asm volatile(
    ".equ ARM_EXC_CURR_SYNC, 0x01\n"
    ".equ ARM_EXC_CURR_IRQ, 0x02\n"
    ".equ ARM_EXC_CURR_FIQ, 0x03\n"
    ".equ ARM_EXC_CURR_ERROR, 0x04\n"
    ".equ ARM_EXC_LOWER_SYNC, 0x11\n"
    ".equ ARM_EXC_LOWER_IRQ, 0x12\n"
    ".equ ARM_EXC_LOWER_FIQ, 0x13\n"
    ".equ ARM_EXC_LOWER_ERROR, 0x14\n"
    "\n"
    ".macro INTERRUPT_ENTRY interrupt_id\n"
    "    nop\n"
    "    \n"
    "    /* this is used for debugging purposes\n"
    "    MOV X0, #\\interrupt_id\n"
    "    MRS X1 , CurrentEL     // current el register\n"
    "    MRS X2 , ESR_EL1       // indicates what exception occured\n"
    "    MRS X3 , FAR_EL1       // contains the fault address\n"
    "    MRS X4 , ELR_EL1\n"
    "    b interruptEntryDebug\n"
    "    */\n"
    "    STP X0 , X1 , [SP, #-16]!\n" // Preserve X0/X1 before using them as scratch registers.
    "\n"
    "    LDR X0, =currentThreadRegisters\n" // Destination for the interrupted context.
    "    LDR X0 , [X0]\n"
    "\n"
    "    STP X2  , X3  , [X0, #16 * 1]\n"
    "    STP X4  , X5  , [X0, #16 * 2]\n"
    "    STP X6  , X7  , [X0, #16 * 3]\n"
    "    STP X8  , X9  , [X0, #16 * 4]\n"
    "    STP X10 , X11 , [X0, #16 * 5]\n"
    "    STP X12 , X13 , [X0, #16 * 6]\n"
    "    STP X14 , X15 , [X0, #16 * 7]\n"
    "    STP X16 , X17 , [X0, #16 * 8]\n"
    "    STP X18 , X19 , [X0, #16 * 9]\n"
    "    STP X20 , X21 , [X0, #16 * 10]\n"
    "    STP X22 , X23 , [X0, #16 * 11]\n"
    "    STP X24 , X25 , [X0, #16 * 12]\n"
    "    STP X26 , X27 , [X0, #16 * 13]\n"
    "    STP X28 , X29 , [X0, #16 * 14]\n"
    "    STR X30 ,       [X0, #16 * 15]\n"
    "\n"
    "    MOV X2  , X0\n"                   // Keep the register-set base after restoring X0.
    "    LDP X0  , X1  , [SP] , #16\n"    // Restore the values saved at vector entry.
    "    STP X0  , X1  , [X2, #16 * 0]\n" // Complete the general-register save.
    "    ADD X2  , X2  , #%c[q_offset]\n" // Pass &Q[0] to the exception entry code.
    "\n"
    "\n"
    "    MOV X0, #\\interrupt_id\n" // First C argument: vector-specific exception ID.
    "\n"
    "    b " PSEUDO_NAKED_TARGET(arm_exception_entry) "\n" // Skip the compiler prologue.
    ".endm\n"
    "\n"
    ".macro INTERRUPT_ENTRY_INVALID\n"
    "\n"
    "B arm_invalid_exception\n"
    "\n"
    ".endm\n"
    "\n"
    ".balign 0x800\n"
    ".global Vector_table_el1\n"
    "Vector_table_el1:\n"
    "curr_el_sp0_sync:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_sp0_irq:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_sp0_fiq:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_sp0_serror:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    "/*  *********************************************  */\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_spx_sync:\n"
    "INTERRUPT_ENTRY ARM_EXC_CURR_SYNC\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_spx_irq:\n"
    "INTERRUPT_ENTRY ARM_EXC_CURR_IRQ\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_spx_fiq:\n"
    "INTERRUPT_ENTRY ARM_EXC_CURR_FIQ\n"
    "\n"
    ".balign 0x80\n"
    "curr_el_spx_serror:\n"
    "INTERRUPT_ENTRY ARM_EXC_CURR_ERROR\n"
    "\n"
    "/*  *********************************************  */\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch64_sync:\n"
    "INTERRUPT_ENTRY ARM_EXC_LOWER_SYNC\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch64_irq:\n"
    "INTERRUPT_ENTRY ARM_EXC_LOWER_IRQ\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch64_fiq:\n"
    "INTERRUPT_ENTRY ARM_EXC_LOWER_FIQ\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch64_serror:\n"
    "INTERRUPT_ENTRY ARM_EXC_LOWER_ERROR\n"
    "\n"
    "/*  *********************************************  */\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch32_sync:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch32_irq:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch32_fiq:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    ".balign 0x80\n"
    "lower_el_aarch32_serror:\n"
    "INTERRUPT_ENTRY_INVALID\n"
    "\n"
    :
    : [q_offset] "i"(__builtin_offsetof(ArchThreadRegisters, Q))
    : "memory"
  );
  __builtin_unreachable();
}
