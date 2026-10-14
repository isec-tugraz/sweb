/**
 * this linker script tells ld how to link and which symbols to add to the
 * kernel.x binary
 */
 
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(entry)
LS_Phys = 0x100000;
LS_Virt = 0xFFFFFFFF80000000;
UpperToLower = 0xFFFFFFFF00000000;
SECTIONS
{
  . = LS_Virt + LS_Phys;

  .text : AT(ADDR(.text) - LS_Virt)
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
    *(.data)
    *(.data.*)
    . = ALIGN(4096);
    *(.gdt_stuff)
    data_end_address = .;
  }

  .tdata ALIGN(16) : AT(ADDR(.tdata) - LS_Virt)
  {
    tls_start = .;
    *(.tdata .tdata.*)
    tls_data_end = .;
  }

  .tbss ALIGN(16) : AT(ADDR(.tbss) - LS_Virt)
  {
    *(.tbss .tbss.*)
    . = ALIGN(16);
    tls_end = .;
  }
  . = tls_end;
  ASSERT(tls_end - tls_start + 8 <= 4096, "Boot CPU TLS exceeds its initial storage")

  .bss ALIGN(4096) : AT(LS_Phys + (LS_Bss - LS_Code))
  {
    LS_Bss = .;
    bss_start_address = .;
    *(.bss)
    *(.bss.*)
    *(COMMON) /* common symbols, usually placed in .bss */
    . = ALIGN(4096);
    bss_end_address = .;

    PROVIDE(kernel_end_address = .);
  }

}
