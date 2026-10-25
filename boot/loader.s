.intel_syntax noprefix
.section .text,"ax",@progbits
.global _start
.global serialWrite
.global cSerialWrite
.global setResulation
.global setMaximumResulation
.global g_vbe_mode_info
.global call16
.global printTL16
.global setTextMode16
.code16

.extern lmain
.extern g_vbe_info
.extern g_vbe_mode_info
.extern vbeIsInited
.extern curVBEMode
.extern ancientTheme

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
    addr32 cmp byte ptr [ancientTheme], 0
    jg skipVBERes
    call setMaximumResulation
skipVBERes:
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

    /* Save the mode's pitch, depth, attributes, and framebuffer address. */
    push cx
    xor ax, ax
    mov es, ax
    mov di, offset g_vbe_mode_info
    mov cx, 128
    cld
    rep stosw
    mov ax, 0x4f01
    mov cx, word ptr [curVBEMode]
    mov di, offset g_vbe_mode_info
    int 0x10
    pop cx

    mov ax, 0x4f02
    mov bx, cx
    or bx, 0x4000              # Request the linear framebuffer.
    int 0x10
    cmp ax, 0x004f
    je .success
.fallback:
    mov ax, 0x0012
    int 0x10
    cmp ax, 0
    jne .failure
    mov word ptr [curVBEMode], 0x0012
    lea si, [ModeSwitchSuccess]
    call serialWrite
    xor ax, ax
    jmp .done
.failure:
    addr32 mov byte ptr [ancientTheme], 0
    lea si, [ModeSwitchFailed]
    call serialWrite
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
    mov ax, 0x0122              # 800x600x32 VBE mode.
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

/*
 * call16(address, args, size) calls a 16-bit far routine below 1 MiB.
 * The routine receives DS:SI = a copy of args at physical 0x1000 and AX =
 * the first word of that copy. Its AX result is returned as a pointer-sized
 * value. The real-mode trampoline and its data are kept in low memory.
 */
.global call16
.type call16, @function
call16:
    push ebp
    mov ebp, esp
    pushfd
    pushad
    cmp dword ptr [ebp + 16], 0x5000
    ja .call16_invalid
    mov eax, dword ptr [ebp + 8]
    cmp eax, 0x000FFFFF
    ja .call16_invalid

    mov dword ptr [call16_saved_esp], esp
    mov esi, dword ptr [ebp + 12]
    mov edi, 0x1000
    mov ecx, dword ptr [ebp + 16]
    cld
    rep movsb

    mov eax, dword ptr [ebp + 8]
    mov edx, eax
    and eax, 0x0000000F
    shr edx, 4
    mov word ptr [0x0800], ax
    mov word ptr [0x0802], dx
    mov ax, word ptr [0x1000]
    cmp dword ptr [ebp + 16], 2
    jae .call16_have_ax
    xor ax, ax
.call16_have_ax:
    mov word ptr [0x0804], ax

    mov esi, offset realModeExecuteStart
    mov edi, 0x0600
    mov ecx, offset realModeExecuteSize
    rep movsb
    mov esi, offset gdtDesc
    mov edi, 0x0806
    mov ecx, 6
    rep movsb
    sgdt [call16_saved_gdtr]
    sidt [call16_saved_idtr]

    mov word ptr [0x080C], 0x03FF
    mov dword ptr [0x080E], 0
    mov dword ptr [0x0812], offset call16ProtectedReturn
    mov word ptr [0x0816], 0x0008

    mov eax, cr0
    mov dword ptr [call16_saved_cr0], eax
    ljmp 0x18, call16_pm16

.call16_invalid:
    popad
    popfd
    pop ebp
    xor eax, eax
    ret

.code16
call16_pm16:
    mov eax, cr0
    and eax, 0x7FFFFFFF       # Disable paging while protected mode is active.
    mov cr0, eax
    mov eax, cr0
    and eax, 0xFFFFFFFE       # Leave protected mode from a 16-bit code segment.
    mov cr0, eax
    ljmp 0x0000, 0x0600

.code16
realModeExecuteStart:
    cli
    xor eax, eax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7A00
    lidt [0x080C]
    mov esi, 0x1000
    mov ax, word ptr [0x0804]
    mov bx, word ptr [0x0800]
    lcall [0x0800]
    cli
    mov word ptr [0x0804], ax
    lgdt [0x0806]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    mov ax, 0x10
    mov ds, ax
    .byte 0x66, 0xFF, 0x2E    # Far jump through a 16:32 pointer.
    .word 0x0812
realModeExecuteEnd:
.set realModeExecuteSize, realModeExecuteEnd - realModeExecuteStart

.code32
call16ProtectedReturn:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    lgdt [call16_saved_gdtr]
    lidt [call16_saved_idtr]
    mov eax, dword ptr [call16_saved_cr0]
    mov cr0, eax
    movzx eax, word ptr [0x0804]
    mov word ptr [call16_return_ax], ax
    mov esp, dword ptr [call16_saved_esp]
    popad
    popfd
    pop ebp
    movzx eax, word ptr [call16_return_ax]
    ret

.type printTL16, @function
.code16
printTL16:
    push bx
    push si
.printTL16_next:
    lodsb
    test al, al
    jz .printTL16_done
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    push ds
    push si
    int 0x10
    pop si
    pop ds
    jmp .printTL16_next
.printTL16_done:
    xor ax, ax
    pop si
    pop bx
    .byte 0x66, 0xCB          # Return with the 16:16 frame from call16.

.code16
setTextMode16:
    mov ax, 0x0003
    int 0x10
    xor ax, ax
    lret

.code32
.bss
.align 4
call16_saved_esp:
    .long 0
call16_saved_cr0:
    .long 0
call16_return_ax:
    .word 0
.align 4
call16_saved_gdtr:
    .space 6
call16_saved_idtr:
    .space 6
.text

protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9F000

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

    .word 0xFFFF    /* 16-bit protected-mode code segment. */
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0x8F      /* 4KB granularity, 16-bit default operand size. */
    .byte 0x00
gdt_end:

gdtDesc:
    .word gdt_end - gdt_start - 1   /* GDT Size (Limit) minus 1 */
    .long gdt_start
