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
    call getMMap

    # .bss is not part of the boot image, so it still holds whatever the BIOS
    # left in RAM.  The mode-selection flag must not be decided by that garbage.
    addr32 mov byte ptr [ancientTheme], 0

    # The video BIOS services below can wait on timer/vsync interrupts, so the
    # BIOS calls must run with interrupts enabled.
    sti
    call vesaInit
    addr32 cmp byte ptr [ancientTheme], 0
    jg skipVBERes
    call setMaximumResulation
skipVBERes:
    # No IDT exists in protected mode yet: IRQs must be off again from here on.
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
        # Only the value 1 means "VBE present"; the BIOS error code that was
        # just reported must not be mistaken for a successful initialisation.
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
    # Reserve a full entry before allowing the BIOS to write it.  Checking
    # DI directly avoids a 16-bit addition wrapping around the buffer end.
    cmp di, MMAP_BASE
    jb .mmap_done
    cmp di, MMAP_END - MMAP_ENTRY_SIZE
    ja .mmap_done

    # Each packed CommonMemoryEntry starts with its 12-byte signature.
    # Copy it into the low-memory record before asking the BIOS to fill the
    # 24-byte E820 payload that follows.
    push di
    mov si, offset regionSignature
    mov cx, 12
    rep movsb
    pop di

    # Request extended attributes.
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

    # Guard against BIOSes that repeat a nonzero continuation token forever.
    test ebx, ebx
    jz .check_length
    cmp ebx, dword ptr [mmap_cont_in]
    je .map_error

    # A 20-byte entry has no extended attributes.
    cmp ecx, 24
    jae .check_length
    mov dword ptr es:[di + 32], 0

.check_length:
    # Keep every successful BIOS region record, including zero-length ones.
    inc word ptr [mmap_count]

.next_entry:
    # Always advance, including for zero-length entries.
    add di, MMAP_ENTRY_SIZE

    # EBX = 0 means the BIOS has no more entries.
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
    # No VBE controller (for example QEMU started with -vga none) means there
    # is no linear framebuffer to drive.  Calling the BIOS mode switch here
    # would wedge on hardware that does not exist, so skip it completely and
    # let the loader continue without a display.
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
 * the first word of that copy. Its AX result is returned as an int. The
 * real-mode trampoline and its data are kept in low memory.
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
    sti
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

# --- temporary debug helper: print EAX as 8 hex digits and ':' on COM1 ---
# Clobbers EAX, EBX, ECX, EDX; needs a usable stack.
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
.align 4096
long_mode_pml4:
    .space 4096
long_mode_pdpt:
    .space 4096
long_mode_pd0:
    .space 16384
long_mode_stack_top:
    .long 0
long_mode_protocol:
    .long 0
long_mode_entry_point:
    .quad 0
.text

protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9F000

    call allocateLoaderStackTop
    test eax, eax
    jz die
    and eax, 0xFFFFFFF0
    mov esp, eax
    push offset die

    jmp lmain

.global prepareLongMode
prepareLongMode:
    cli
    mov eax, dword ptr [esp + 12]
    mov dword ptr [long_mode_stack_top], eax
    mov eax, dword ptr [esp + 16]
    mov dword ptr [long_mode_protocol], eax
    mov eax, dword ptr [esp + 4]
    mov dword ptr [long_mode_entry_point], eax
    mov eax, dword ptr [esp + 8]
    mov dword ptr [long_mode_entry_point + 4], eax

    xor eax, eax
    mov edi, offset long_mode_pml4
    mov ecx, 6144
    cld
    rep stosd

    mov eax, offset long_mode_pdpt
    or eax, 3
    mov dword ptr [long_mode_pml4], eax
    mov eax, offset long_mode_pd0
    or eax, 3
    mov dword ptr [long_mode_pdpt], eax
    mov eax, offset long_mode_pd0 + 4096
    or eax, 3
    mov dword ptr [long_mode_pdpt + 8], eax
    mov eax, offset long_mode_pd0 + 8192
    or eax, 3
    mov dword ptr [long_mode_pdpt + 16], eax
    mov eax, offset long_mode_pd0 + 12288
    or eax, 3
    mov dword ptr [long_mode_pdpt + 24], eax

    mov edi, offset long_mode_pd0
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
    mov eax, offset long_mode_pml4
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
    addr32 mov esp, dword ptr [long_mode_stack_top]
    and rsp, -16
    # Reserve the protocol block inside the stack instead of above its top so
    # the copy can never scribble over somebody else's allocation.
    sub rsp, 64
    mov rdi, rsp
    addr32 mov esi, dword ptr [long_mode_protocol]
    mov ecx, 6
    cld
    rep movsq
    # rep movsq advanced rdi past the block; the SysV ABI wants rdi to point
    # at the (MEMORY class) EBootProtocol argument the kernel is about to read.
    mov rdi, rsp
    xor ebp, ebp
    addr32 call qword ptr [long_mode_entry_point]
.long_mode_halt:
    cli
    hlt
    jmp .long_mode_halt
.code32

.code64
.global handleSyscall64
.type handleSyscall64, @function
handleSyscall64:
    mov eax, 1
    ret
.size handleSyscall64, .-handleSyscall64
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

.global g_vbe_mode_info
hexDigits:
    .byte '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
hexBuffer:
    .space 5

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