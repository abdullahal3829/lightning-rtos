.intel_syntax noprefix
.section .text,"ax",@progbits
.global _start
.global serialWrite
.global cSerialWrite
.global setResulation
.global setMaximumResulation
.global g_vbe_mode_info
.code16

.extern lmain
.extern g_vbe_info
.extern g_vbe_mode_info
.extern vbeIsInited
.extern curVBEMode

.section .text._start,"ax",@progbits
_start:
    mov bootdrive, dl
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7E00

    call serialInit
    lea si, [BTSuccess]
    call serialWrite
    
    call vesaInit
    call setMaximumResulation

    lgdt [gdtDesc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ljmp 0x08, protected_mode_entry

vesaInit:
	clc
    xor ax, ax
    mov es, ax
    mov di, offset g_vbe_info
    mov ax, 0x4f00
	int 0x10
	cmp ax, 0x004f
	jne .failed
    mov dword ptr [vbeIsInited], 1
    lea si, [VBESuccess]
    call serialWrite
	ret
	.failed:
		stc
        movzx eax, ax
        mov dword ptr [vbeIsInited], eax
        lea si, [VBEFailure]
        call serialWrite
        lea si, [VBEErrorHexPrefix]
        call serialWrite
        mov ax, word ptr [vbeIsInited]
        call serialWriteHex
        lea si, [VBENewLine]
        call serialWrite
		ret

setResulation:    
    push bp
    mov bp, sp
    mov cx, word ptr [bp + 4]
    mov ax, word ptr [vbeIsInited]
    cmp ax, 1
    jl .not_inited

    mov word ptr [curVBEMode], cx
    mov ax, 0x4f02
    mov bx, cx
    int 0x10
    cmp ax, 0x004f
    je .success
.fallback:
    mov ax, 0x0012
    int 0x10
    mov word ptr [curVBEMode], 0x0012
    lea si, [ModeSwitchSuccess]
    call serialWrite
    xor ax, ax
    jmp .done

.done:
    leave
    ret 2

.success:
    mov word ptr [curVBEMode], cx
    xor ax, ax
    leave
    ret 2

.not_inited:
    jmp .fallback

setMaximumResulation:
    mov ax, 0x0124
    push ax
    call setResulation
    ret

serialInit:
    xor ax, ax
    mov ds, ax
    xor dx, dx

.loop:
    mov bx, dx
    shl bx, 1
    mov ax, [0x0400 + bx] 
    test ax, ax
    jz .skip

    mov ah, 0x00
    mov al, 0xE3
    int 0x14

.skip:
    inc dx
    cmp dx, 4
    jl .loop

    ret

cSerialWrite:
    push bp
    mov bp, sp
    mov si, word ptr [bp + 4]
    call serialWrite
    pop bp
    ret

serialWrite:
    push ax
    push bx
    push cx
    push dx
    push si

    mov bx, 0
    mov cx, 4

serialWritePortLoop:
    mov dx, word ptr [0x0400 + bx]
    cmp dx, 0
    je serialWriteNextPort

    mov si, si
    mov al, byte ptr [si]
    test al, al
    jz serialWriteDone

serialWriteCharLoop:
    mov dx, word ptr [0x0400 + bx]
    mov al, byte ptr [si]
    test al, al
    jz serialWriteNextPort
    push ax
    add dx, 5
serialWriteWait:
    in al, dx
    test al, 0x20
    jz serialWriteWait
    sub dx, 5
    pop ax
    out dx, al
    inc si
    jmp serialWriteCharLoop

serialWriteNextPort:
    add bx, 2
    loop serialWritePortLoop

serialWriteDone:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

serialWriteHex:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    lea di, [hexBuffer + 4]
    mov byte ptr [di], 0
    mov cx, 4

serialWriteHexLoop:
    dec di
    mov bx, ax
    and bx, 0x000F
    mov dl, byte ptr [hexDigits + bx]
    mov byte ptr [di], dl
    shr ax, 4
    loop serialWriteHexLoop

    mov si, di
    call serialWrite

    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.code32
.global outb
.type outb, @function
outb:
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8]    # port
    mov eax, [ebp + 12]   # value
    out dx, al

    pop ebp
    ret

.global inb
.type inb, @function
inb:
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8]    # port
    xor eax, eax
    in al, dx

    pop ebp
    ret

protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9C00

    jmp lmain

BTSuccess:
    .asciz "Launched Rocket\r\n"
ModeSwitchSuccess:
    .asciz "Mode Switch Successfully\r\n"
ModeSwitchFailed:
    .asciz "Mode Switch Failed\r\n"
VBESuccess:
    .asciz "VBE Is Initialized Successfully\r\n"
VBEFailure:
    .asciz "VBE Is Failed Initialized With Error Code: "
VBEErrorHexPrefix:
    .asciz "0x"
VBENewLine:
    .asciz "\n"

.global g_vbe_mode_info
hexDigits:
    .byte '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
hexBuffer:
    .space 5

g_vbe_mode_info:
    .space 256

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
