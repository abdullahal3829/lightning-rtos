.intel_syntax noprefix
.section .text
.global _start
.code16

.extern lmain
_start:
    mov bootdrive, dl
    cli
    lgdt [gdtDesc]
    mov eax, cr0
    or al, 1
    mov cr0, eax

    .byte 0xEA
    .word lmain
    .word 0x8

bootdrive:
    .byte 0

.balign 4
gdt_start:
    .quad 0x0000000000000000

    .word 0xFFFF    /* Limit bits 0-15 */
    .word 0x0000    /* Base bits 0-15 */
    .byte 0x00      /* Base bits 16-23 */
    .byte 0x9A      /* Access byte (Present, Ring 0, Code, Executable/Readable) */
    .byte 0xCF      /* Flags (4KB blocks, 32-bit mode) + Limit bits 16-19 */
    .byte 0x00      /* Base bits 24-31 */

    .word 0xFFFF    /* Limit bits 0-15 */
    .word 0x0000    /* Base bits 0-15 */
    .byte 0x00      /* Base bits 16-23 */
    .byte 0x92      /* Access byte (Present, Ring 0, Data, Writable) */
    .byte 0xCF      /* Flags + Limit bits 16-19 */
    .byte 0x00      /* Base bits 24-31 */
gdt_end:

gdtDesc:
    .word gdt_end - gdt_start - 1   /* GDT Size (Limit) minus 1 */
    .long gdt_start  