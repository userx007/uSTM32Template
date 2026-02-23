/*
 * STM32F4xx.ld — Linker script for STM32F411xE
 * Flash : 512 KB @ 0x08000000
 * SRAM  : 128 KB @ 0x20000000
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Minimum sizes — adjust to your application needs */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

SECTIONS
{
  /*------------------------------------------------------------
   * 1. Vector table + code in Flash
   *----------------------------------------------------------*/
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ARM exception unwind tables */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM        : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* C++ init/fini arrays (needed by __libc_init_array) */
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  /*------------------------------------------------------------
   * 2. Initialised data: stored in Flash, copied to SRAM
   *    by Reset_Handler using _sidata, _sdata, _edata
   *----------------------------------------------------------*/
  _sidata = LOADADDR(.data);   /* Flash load address (LMA) */

  .data :
  {
    . = ALIGN(4);
    _sdata = .;                /* SRAM start (VMA) */
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;                /* SRAM end   (VMA) */
  } >SRAM AT>FLASH

  /*------------------------------------------------------------
   * 3. Zero-initialised data in SRAM
   *    cleared by Reset_Handler using _sbss, _ebss
   *----------------------------------------------------------*/
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >SRAM

  /*------------------------------------------------------------
   * 4. ThreadX internal sections (if any port uses these)
   *----------------------------------------------------------*/
  .threadx_data (NOLOAD) :
  {
    . = ALIGN(4);
    *(.threadx_data)
    . = ALIGN(4);
  } >SRAM

  /*------------------------------------------------------------
   * 5. Heap and Stack — verify they fit
   *----------------------------------------------------------*/
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(_end = .);         /* used by _sbrk() in syscalls.c */
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >SRAM

  /* Discard C++ exception handling if unused */
  /DISCARD/ :
  {
    libc.a  (*)
    libm.a  (*)
    libgcc.a(*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}