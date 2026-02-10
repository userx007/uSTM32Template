/*
 * Standalone linker script for STM32F411CEU6
 * GCC 14.2+ compatible - handles .eh_frame sections properly
 * 512k Flash, 128k RAM
 * Updated to silence orphan section warnings
 */

MEMORY
{
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Define stack and heap sizes */
_stack_size = 0x400;  /* 1KB stack */
_heap_size = 0x200;   /* 512B heap */

/* Entry point */
ENTRY(reset_handler)

SECTIONS
{
    .text : {
        *(.vectors)     /* Vector table */
        *(.text*)       /* Program code */
        *(.rodata*)     /* Read-only data */
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        
        /* C++ support */
        KEEP(*(.init))
        KEEP(*(.fini))
        
        /* Handle linker-generated veneers */
        *(.vfp11_veneer)
        *(.v4_bx)
        
        . = ALIGN(4);
        _etext = .;
    } >rom

    /* C++ exception handling (required by GCC 14.2+ crtbegin.o) */
    .ARM.extab : ALIGN(4) {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >rom
    
    .ARM.exidx : ALIGN(4) {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } >rom

    /* Exception frames - CRITICAL for GCC 14.2+ */
    .eh_frame_hdr : ALIGN(4) {
        KEEP(*(.eh_frame_hdr))
        KEEP(*(.eh_frame_entry))
    } >rom
    
    .eh_frame : ALIGN(4) {
        PROVIDE(__eh_frame_start = .);
        KEEP(*(.eh_frame))
        PROVIDE(__eh_frame_end = .);
    } >rom

    /* C++ constructors/destructors */
    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >rom
    
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >rom
    
    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >rom

    /* Handle clone table for C++ (from crtbegin/crtend) */
    .tm_clone_table : ALIGN(4) {
        *(.tm_clone_table)
    } >rom

    /* Handle indirect PLT sections (static linking compatibility) */
    .iplt : ALIGN(4) {
        *(.iplt)
    } >rom

    .igot.plt : ALIGN(4) {
        *(.igot.plt)
    } >rom

    /* Handle dynamic relocations (if any) */
    .rel.dyn : ALIGN(4) {
        *(.rel.iplt)
        *(.rel.dyn)
    } >rom

    /* Initialized data */
    .data : {
        _data = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >ram AT >rom
    _data_loadaddr = LOADADDR(.data);

    /* Uninitialized data */
    .bss : {
        _bss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >ram

    /* Heap */
    .heap (NOLOAD) : {
        . = ALIGN(4);
        _heap_start = .;
        . = . + _heap_size;
        . = ALIGN(4);
        _heap_end = .;
    } >ram

    /* Stack */
    .stack (NOLOAD) : {
        . = ALIGN(8);
        . = . + _stack_size;
        . = ALIGN(8);
        _stack = .;
    } >ram

    /* End of RAM */
    end = .;
    _end = .;
    __end__ = .;

    /* Debug sections - keep but don't load */
    .debug_info      0 : { *(.debug_info) }
    .debug_abbrev    0 : { *(.debug_abbrev) }
    .debug_loclists  0 : { *(.debug_loclists) }
    .debug_aranges   0 : { *(.debug_aranges) }
    .debug_rnglists  0 : { *(.debug_rnglists) }
    .debug_macro     0 : { *(.debug_macro) *(.debug_macro[*]) }
    .debug_line      0 : { *(.debug_line) }
    .debug_str       0 : { *(.debug_str) }
    .debug_frame     0 : { *(.debug_frame) }
    .debug_line_str  0 : { *(.debug_line_str) }

    /* Remove other sections */
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.ARM.attributes)
        /* NOTE: Do NOT discard .eh_frame* here! */
    }
}

/* Provide symbols for stack pointer */
PROVIDE(_stack_ptr = ORIGIN(ram) + LENGTH(ram));
