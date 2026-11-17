/**
 * this linker script tells ld how to link and which symbols to add to the
 * kernel.x binary
 */

ENTRY(entry)
LS_Virt = 0xFFFFFFC000000000;
LS_Phys = 0x0000000000000000;
/* Specify the memory areas */

SECTIONS
{
  .text LS_Virt : AT(LS_Phys)
  {
    PROVIDE(kernel_start_address = ABSOLUTE(.));

    LS_Code = .;

    text_start_address = .;
    *(.text.boot)
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
    . = ALIGN(8);
    __preinit_array_start = .;
    KEEP(*(.preinit_array))
    __preinit_array_end = .;
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
    *(.data*)
    data_end_address = .;
  }
  
  .bss ALIGN(4096) : AT(LS_Phys + (LS_Bss - LS_Code))
  {
    LS_Bss = .;
    bss_start_address = .;
    *(.bss*)
    . = ALIGN(4096);
    *(COMMON)
        . = ALIGN(4096);
    bss_end_address = .;

    PROVIDE(kernel_end_address = .);
  }
  
  .ARM.attributes 0 : { *(.ARM.attributes) }
  .comment 0 : { *(.comment) }
}
