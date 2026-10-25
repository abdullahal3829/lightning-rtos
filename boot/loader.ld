ENTRY(_start)
SECTIONS
{
    .text._start 0x7E00 : {
        *(.text._start)
    }
    .text.loader : {
        *(.text)
    }
    .lowdata : {
        *(.lowdata)
    }
    .text : {
        *(.text .text.*)
        *(.rodata*)
    }
    .data : {
        *(.data .data.*)
    }
    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }
}
