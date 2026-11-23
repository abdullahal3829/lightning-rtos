# Part Public License (PPL)
# Copyright (C) 2026 Abdullah Al Nahian/Raiyan
# View LICENSE.md

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
.global die
.code16

.set MMAP_BASE,         0x0500
.set MMAP_END,          0x7C00
.set MMAP_ENTRY_SIZE,   36

.extern lmain
.extern g_vbe_info
.extern g_vbe_mode_info
.extern vbeIsInited
.extern curVBEMode
.extern ancientTheme
.extern headlessMode
.extern allocateLoaderStackTop

.section .text._start,"ax",@progbits
_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7E00
    mov byte ptr [bootdrive], dl

    call serialInit
    lea si, [BTSuccess]
    call serialWrite
    call getMMap
    lea si, [MsgMMapDone]
    call serialWrite
    call vesaInit
    cmp dword ptr [vbeIsInited], 1
    js .skipVBE
    call setMaximumResulation
.skipVBE:
    # Read /oskrnl from the ISO using El Torito CD-sector addressing.
    mov dl, byte ptr [bootdrive]
    mov word ptr [kernel_dap + 2], 6
    mov word ptr [kernel_dap + 4], 0
    mov word ptr [kernel_dap + 6], 0x9000
    mov dword ptr [kernel_dap + 8], 1073
    mov dword ptr [kernel_dap + 12], 0
    lea si, [kernel_dap]
    mov ah, 0x42
    int 0x13
    jc .kernel_read_failed

    lea si, [MsgKernelLoaded]
    call serialWrite
    jmp enter_protected_mode

.kernel_read_failed:
    lea si, [MsgKernelReadFail]
    call serialWrite
    cli
    hlt
    jmp .kernel_read_failed

enter_protected_mode:
    cli
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
        mov dword ptr [vbeIsInited], 0
		ret

getMMap:
    push ds
    push es

    xor ax, ax
    mov ds, ax
    mov es, ax

    mov word ptr [mmap_count], 0
    xor ebx, ebx
    mov di, MMAP_BASE

.map_loop:
    cmp di, MMAP_BASE
    jb .mmap_done
    cmp di, MMAP_END - MMAP_ENTRY_SIZE
    ja .mmap_done

    push di
    mov si, offset regionSignature
    mov cx, 12
    rep movsb
    pop di

    mov dword ptr es:[di + 32], 1

    mov dword ptr [mmap_cont_in], ebx
    mov eax, 0xE820
    mov edx, 0x534D4150       # 'SMAP'
    mov ecx, 24
    push di
    add di, 12
    int 0x15
    pop di
    jc .map_error

    cmp eax, 0x534D4150
    jne .map_error

    cmp ecx, 20
    jb .map_error

    test ebx, ebx
    jz .check_length
    cmp ebx, dword ptr [mmap_cont_in]
    je .map_error

    cmp ecx, 24
    jae .check_length
    mov dword ptr es:[di + 32], 0

.check_length:
    inc word ptr [mmap_count]

.next_entry:
    add di, MMAP_ENTRY_SIZE

    test ebx, ebx
    jnz .map_loop

    mov ax, [mmap_count]
    clc
    jmp .mmap_done

.map_error:
    mov ax, [mmap_count]
    stc

.mmap_done:
    pop es
    pop ds
    ret

setResulation:
    push bp
    mov bp, sp
    mov cx, word ptr [bp + 4]
    mov ax, word ptr [vbeIsInited]
    cmp ax, 1
    jl .not_inited

    mov word ptr [curVBEMode], cx

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
    push es
    push di
    push cx
    xor ax, ax
    mov es, ax
    mov di, offset g_vbe_mode_info
    mov cx, 128
    cld
    rep stosw
    pop cx
    pop di
    pop es
    mov ax, 0x0012
    int 0x10
    mov ah, 0x0F
    int 0x10
    cmp al, 0x12
    jne .failure
    mov word ptr [curVBEMode], 0x0012
    lea si, [ModeSwitchSuccess]
    call serialWrite
    xor ax, ax
    jmp .resulation_done
.failure:
    addr32 mov byte ptr [ancientTheme], 0
    lea si, [ModeSwitchFailed]
    call serialWrite
.resulation_done:
    leave
    ret 2

.success:
    mov word ptr [curVBEMode], cx
    xor ax, ax
    leave
    ret 2

.not_inited:
    addr32 mov byte ptr [headlessMode], 1
    lea si, [NoDisplayAvailable]
    call serialWrite
    leave
    ret 2

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

# Print AL as one hex digit to COM1 (int 0x10, ah=0x0E)
print_hex_digit:
    and al, 0x0F
    cmp al, 10
    jb .digit
    add al, 7              # 'A' - 10 = 7
.digit:
    add al, '0'
    jmp print_char_direct

# Print AL as two hex digits to COM1
print_hex_byte:
    push ax
    mov ah, al
    shr al, 4
    call print_hex_digit
    call print_char_direct
    mov al, ah
    call print_hex_digit
    call print_char_direct
    pop ax
    ret

# Direct COM1 char output (int 0x10, ah=0x0E)
print_char_direct:
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
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

.code16
call16:
    ret  # stub - not used for BIOS disk I/O

.code16
printTL16:
    ret

.code16
setTextMode16:
    ret

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

.global prepareLongMode
.set LM_PML4, 0x00100000
.set LM_PDPT, 0x00101000
.set LM_PD0, 0x00102000
.set LM_STACK_TOP, 0x00106000
.set LM_PROTOCOL, 0x00106008
.set LM_ENTRY_POINT, 0x00106010
prepareLongMode:
    cli
    mov eax, dword ptr [esp + 4]
    mov dword ptr [LM_ENTRY_POINT], eax
    mov eax, dword ptr [esp + 8]
    mov dword ptr [LM_ENTRY_POINT + 4], eax

    mov eax, dword ptr [esp + 12]
    mov dword ptr [LM_STACK_TOP], eax
    mov eax, dword ptr [esp + 16]
    mov dword ptr [LM_STACK_TOP + 4], eax

    mov eax, dword ptr [esp + 20]
    mov dword ptr [LM_PROTOCOL], eax
    mov eax, dword ptr [esp + 24]
    mov dword ptr [LM_PROTOCOL + 4], eax

    xor eax, eax
    mov edi, LM_PML4
    mov ecx, 6144
    cld
    rep stosd

    mov eax, LM_PDPT
    or eax, 3
    mov dword ptr [LM_PML4], eax
    mov eax, LM_PD0
    or eax, 3
    mov dword ptr [LM_PDPT], eax
    mov eax, LM_PD0 + 4096
    or eax, 3
    mov dword ptr [LM_PDPT + 8], eax
    mov eax, LM_PD0 + 8192
    or eax, 3
    mov dword ptr [LM_PDPT + 16], eax
    mov eax, LM_PD0 + 12288
    or eax, 3
    mov dword ptr [LM_PDPT + 24], eax

    mov edi, LM_PD0
    mov ecx, 2048
    mov eax, 0x83
.long_mode_map_loop:
    mov dword ptr [edi], eax
    mov dword ptr [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .long_mode_map_loop

    mov eax, cr4
    or eax, 0x30
    mov cr4, eax
    mov eax, LM_PML4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    push 0x20
    push offset long_mode_entry
    retf

long_mode_entry:
.code64
    .byte 0x66, 0xB8, 0x10, 0x00
    .byte 0x8E, 0xD8
    .byte 0x8E, 0xC0
    .byte 0x8E, 0xD0
    addr32 mov esp, dword ptr [LM_STACK_TOP]
    and rsp, -16
    sub rsp, 64
    mov rdi, rsp
    addr32 mov esi, dword ptr [LM_PROTOCOL]
    mov ecx, 6
    cld
    rep movsq
    mov rdi, rsp
    xor ebp, ebp
    addr32 call qword ptr [LM_ENTRY_POINT]
.long_mode_halt:
    cli
    hlt
    jmp .long_mode_halt
.code32

die:
    sti
dieLoop:
    hlt
    jmp dieLoop

BTSuccess:
    .asciz "Launched Rocket\r\n"
ModeSwitchSuccess:
    .asciz "Mode Switch Successfully\r\n"
ModeSwitchFailed:
    .asciz "Mode Switch Failed\r\n"
NoDisplayAvailable:
    .asciz "VBE controller unavailable; continuing without a display.\r\n"
VBESuccess:
    .asciz "VBE Is Initialized Successfully\r\n"
VBEFailure:
    .asciz "VBE Is Failed Initialized With Error Code: "
VBEErrorHexPrefix:
    .asciz "0x"
VBENewLine:
    .asciz "\n"
regionSignature:
    .asciz "BTLDRREGION"
MsgMMapDone:
    .asciz "MMap Done\r\n"
MsgKernelLoaded:
    .asciz "Kernel Loaded\r\n"
MsgKernelReadFail:
    .asciz "Kernel Read Fail\r\n"
MsgBootDrive:
    .asciz "BootDrive="
MsgSpt:
    .asciz "SPT="
MsgComma:
    .asciz ","
MsgChs:
    .asciz "CHS="
MsgCRLF:
    .asciz "\r\n"

.align 4
kernel_dap:
    .byte 0x10, 0
    .word 6
    .word 0, 0x9000
    .quad 1073

.global g_vbe_mode_info
hexDigits:
    .byte '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
hexBuffer:
    .space 5

.section .lowdata
.align 4
g_vbe_mode_info:
    .space 256

.global bootdrive
bootdrive:
    .byte 0

mmap_count:
    .long 0
.global mmap_count
mmap_cont_in:
    .long 0

# GDT and its descriptor must be below 64 KiB so real-mode lgdt can access them.
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

    .word 0xFFFF    /* 64-bit long-mode code segment. */
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0xAF      /* 4KB granularity, 64-bit code, default size clear. */
    .byte 0x00
gdt_end:

gdtDesc:
    .word gdt_end - gdt_start - 1   /* GDT Size (Limit) minus 1 */
    .long gdt_start

.section .text
