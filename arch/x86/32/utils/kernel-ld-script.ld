/**
 * this linker script tells ld how to link and which symbols to add to the
 * kernel.x binary
 */
 
/* let the linker use its 'native' format  (ELF/COFF/PE) */
OUTPUT_FORMAT("elf32-i386")
ENTRY(entry)
LS_Phys = 0x100000; /* 1 meg */
LS_Virt = 0x80000000; /* 2 gig + 1 meg due to 4m pages for kernel area*/
SECTIONS
{
  .text LS_Virt : AT(LS_Phys)
  {
    PROVIDE(kernel_start_address = ABSOLUTE(.));

    LS_Code = .;

    text_start_address = .;
    *(.mboot)
    *(.text)
    *(.text.*)
    text_end_address = .;
    *(.gnu.linkonce.r.*)
    ro_data_start_address = .;
    *(.rodata*)
    ro_data_end_address = .;
    . = ALIGN(4096);
  }

  .data  ALIGN(4096) : AT(LS_Phys + (LS_Data - LS_Code))
  {
    LS_Data = .;
    data_start_address = .;
    *(.data)
    . = ALIGN(4096);
    *(.gdt_stuff)
    . = ALIGN(4096);
    *(COMMON) /* common symbols, usually placed in .bss */
    data_end_address = .;
    . = ALIGN(4096);
  }

  .bss : AT(LS_Phys + (LS_Bss - LS_Code))
  {
    LS_Bss = .;
    bss_start_address = .;
    *(.bss)
    . = ALIGN(4096);
    bss_end_address = .;

    PROVIDE(kernel_end_address = .);
  }

}