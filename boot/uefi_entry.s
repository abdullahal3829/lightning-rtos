# Part Public License (PPL)
# Copyright (C) 2026 Abdullah Al Nahian/Raiyan
# View LICENSE.md
#
# UEFI entry stub.
#
# OVMF with El Torito no-emulation boot jumps to offset 0 of the loaded PE
# image instead of respecting AddressOfEntryPoint.  This stub is linked at
# VMA 0x0 so that offset 0 contains a far jump to the real efi_main.

.intel_syntax noprefix
.section .text.efi_entry,"ax",@progbits
.code64
.global efi_entry
efi_entry:
    # Jump to efi_main.  The linker resolves the address.
    jmp efi_main
