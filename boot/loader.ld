ENTRY(_start)
SECTIONS
{
    . = 0x7E00;
    .text : {
        _start = .;
        *(.text._start)
        *(.text)
        *(.rodata*)
    }
    .data : {
        *(.data)
    }
    .bss : {
        *(.bss)
    }
}