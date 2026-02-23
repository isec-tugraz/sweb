/**
 * this linker script tells ld how to link and which symbols to add to the
 * kernel.x binary
 */

ENTRY(entry)
LS_Virt = 0x80000000;
LS_Phys = 0x0000;
/* Specify the memory areas */

SECTIONS
{
  .text LS_Virt : AT(LS_Phys) 
  {
    PROVIDE(kernel_start_address = ABSOLUTE(.));

    LS_Code = .;

    text_start_address = .;
    *(.text)
    *(.text.*)
    text_end_address = .;
    ro_data_start_address = .;
    *(.rodata*)
    ro_data_end_address = .;
    swebdbg_start_address_nr  = .;
    *(.swebdbg)
    swebdbg_end_address_nr  = .;
  }
  
  .data ALIGN(4096) : AT(LS_Phys + (LS_Data - LS_Code))
  {
    LS_Data = .;
    data_start_address = .;
    *(.data*)
    data_end_address = .;
  }
  
  .bss ALIGN(4096) : AT(LS_Phys + (LS_Bss - LS_Code))
  {
    LS_Bss = .;
    bss_start_address = .;
    *(.bss)
    . = ALIGN(4096);
    *(COMMON)
    bss_end_address = .;

    PROVIDE(kernel_end_address = .);
  }
  
  .multicore : AT(LS_Phys + (LS_Multicore - LS_Code))
  {
    LS_Multicore = .;
    multicore_start_address = .;
    . = ALIGN(4096);
    *(.multicore)
    . = ALIGN(4096);
    multicore_end_address = .;
  }
  
  .ARM.attributes 0 : { *(.ARM.attributes) }
  .comment 0 : { *(.comment) }
}
