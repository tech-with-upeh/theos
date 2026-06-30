OUTPUT_FORMAT("binary")
ENTRY(main)              /* Tell the linker main is the boss */

SECTIONS
{
    . = 0x00400000;

    .text : {
        *(.text.main)    /* Force main to the absolute front of the file */
        *(.text)
        *(.text.*)
    }

    .rodata : { *(.rodata) *(.rodata.*) }
    .data   : { *(.data) }
    .bss    : { *(.bss) *(COMMON) }
}
