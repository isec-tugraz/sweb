// Adapted from Florian Kargl's arch_apstartup.S in #287.
extern "C" __attribute__((naked, used)) void apStartup()
{
  asm volatile(
      ".global ap_boot_start, ap_boot_end, ap_boot_parameters\n"
      "ap_boot_start:\n"
      ".code16\n"
      "cli\n"
      "cld\n"
      "xorw %ax, %ax\n"
      "movw %ax, %ds\n" // Physical addresses below 64 KiB, including the copied startup code at 0x8000.
      "movw %ax, %es\n"
      "movw %ax, %ss\n"
      "lgdt 0x8000 + (ap_boot_gdtr - ap_boot_start)\n"
      "movl %cr4, %eax\n"
      "orl $0x630, %eax\n" // PAE, PSE, OSFXSR and OSXMMEXCPT, like the boot CPU.
      "movl %eax, %cr4\n"
      "movl 0x8000 + (ap_boot_parameters - ap_boot_start), %eax\n"
      "movl %eax, %cr3\n"
      "movl $0xC0000080, %ecx\n" // EFER.
      "rdmsr\n"
      "orl $0x900, %eax\n" // Enable long mode and NX.
      "wrmsr\n"
      "movl %cr0, %eax\n"
      "andl $0xFFFFFFF3, %eax\n" // Clear EM and TS.
      "orl $0x80010003, %eax\n" // Protected mode, paging, write protection and FPU monitor.
      "movl %eax, %cr0\n"
      "ljmpl $0x10, $0x8000 + (ap_long_mode - ap_boot_start)\n"
      ".code64\n"
      "ap_long_mode:\n"
      "movw $0x20, %ax\n" // Kernel data selector.
      "movw %ax, %ds\n"
      "movw %ax, %es\n"
      "movw %ax, %ss\n"
      "movw %ax, %fs\n"
      "movw %ax, %gs\n"
      "movq 0x8000 + (ap_boot_parameters - ap_boot_start) + 8, %rsp\n" // Private stack, aligned to 16 bytes.
      "pushq $0\n" // C function entry alignment and a terminating backtrace return address.
      "xorq %rbp, %rbp\n"
      "movq 0x8000 + (ap_boot_parameters - ap_boot_start) + 16, %rdi\n" // CPU* argument.
      "movq 0x8000 + (ap_boot_parameters - ap_boot_start) + 24, %rax\n"
      "jmp *%rax\n" // Enter ordinary C++ at its linked kernel address.
      ".balign 8\n"
      "ap_boot_gdt:\n"
      ".quad 0, 0, 0x00AF9A000000FFFF, 0, 0x00CF92000000FFFF\n" // Selectors 0x10 (code) and 0x20 (data).
      "ap_boot_gdtr:\n"
      ".word 39\n"
      ".long 0x8000 + (ap_boot_gdt - ap_boot_start)\n"
      ".balign 8\n"
      "ap_boot_parameters:\n"
      ".zero 32\n" // Filled in by the boot CPU before each INIT/SIPI sequence.
      "ap_boot_end:\n");
}
