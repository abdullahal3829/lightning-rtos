/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 *
 * Linker script for the UEFI (x86-64) bootloader image.
 *
 * OVMF loads UEFI applications from FAT via AddressOfEntryPoint.
 * The .text section starts at VMA 0x0.
 */

ENTRY(efi_main)

SECTIONS
{
    . = 0x0;

    .text : {
        *(.text .text.*) *(.rodata*) *(.note.gnu.property)
        . = ALIGN(0x20);
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.*) *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame *) *(.eh_frame_hdr *)
        *(.comment)
    }
}
