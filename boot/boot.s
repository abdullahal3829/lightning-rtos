.intel_syntax noprefix
.section .text
.global _start
.code16

_start:
    cli                     # Disable interrupts during setup
    xor ax, ax
    mov ds, ax              # Data segment = 0
    mov es, ax              # Extra segment = 0
    mov ss, ax              # Stack segment = 0
    mov sp, 0x7C00          # Stack grows below the boot sector
    cld                     # String instructions go forward
    mov [bootdrive], dl     # Save boot drive index
    sti                     # Re-enable interrupts

    # Enable A20 Line via Fast A20 Gate
    in al, 0x92
    or al, 2
    out 0x92, al

    # Try EDD Extensions (LBA) first
    mov dl, [bootdrive]
    mov ah, 0x41            # "check extensions present" service
    mov bx, 0x55aa
    int 0x13
    jc try_chs
    cmp bx, 0xaa55
    jne try_chs
    test cx, 1               # Check support for the "fixed disk access subset"
    jz try_chs

edd_ok:
    # Read 960 sectors in fifteen EDD transfers; each transfer is 32 KB.
    # The linked LVGL font data makes the second-stage loader exceed 896 sectors.
    mov byte ptr [read_chunks], 15
read_chunk:
    mov dl, [bootdrive]
    mov word ptr [dap + 2], 64   # BIOS may update the transferred-sector count.
    lea si, [dap]           # DS:SI points to the packet
    mov ah, 0x42            # Extended Read
    int 0x13
    jc try_chs
    add word ptr [dap + 6], 0x0800  # Advance destination by 32 KB.
    add word ptr [dap + 8], 64      # Advance LBA by 64 sectors.
    dec byte ptr [read_chunks]
    jnz read_chunk
    jmp read_ok

try_chs:
    # BIOS CD boot emulation may not offer EDD. Read all 960 sectors with
    # one-sector CHS requests using the standard 1.44 MB floppy geometry.
    mov word ptr [chs_lba], 1
    mov word ptr [chs_left], 960
    mov word ptr [chs_segment], 0x07e0
chs_read:
    mov ax, word ptr [chs_lba]
    xor dx, dx
    mov cx, 18
    div cx                  # AX=track, DX=sector index (18 sectors/track)
    inc dl
    mov byte ptr [chs_sector], dl
    xor dx, dx
    mov cx, 2
    div cx                  # AX=cylinder, DX=head (2 heads/cylinder)
    mov ch, al
    mov dh, dl
    mov cl, byte ptr [chs_sector]
    mov ax, word ptr [chs_segment]
    mov es, ax
    xor bx, bx
    mov dl, byte ptr [bootdrive]
    mov ax, 0x0201           # Read one sector
    int 0x13
    jc diskerr
    inc word ptr [chs_lba]
    add word ptr [chs_segment], 0x20
    dec word ptr [chs_left]
    jnz chs_read
    jmp read_ok

read_ok:
    # Enter stage two; it prints its own success message at its entry point.
    mov dl, [bootdrive]
    jmp 0x0000:0x7E00

print:
    lodsb                   # Load byte from DS:SI into AL
    test al, al
    jz print_done

    mov ah, 0x0E            # BIOS teletype function
    mov bh, 0x00            # Display page 0
    int 0x10                # BIOS video interrupt
    jmp print

print_done:
    ret

die:
    hlt
    jmp die

diskerr:
    lea si, [dskerrMSG]
    call print
    jmp die

incap:
    lea si, [incapMSG]
    call print
    jmp die

# -------------------------------------------------------------------------
# Data Section
# -------------------------------------------------------------------------
dskerrMSG:
    .asciz "Disk Fault!"

incapMSG:
    .asciz "Not Supported."

bootdrive:
    .byte 0
read_chunks:
    .byte 0
chs_lba:
    .word 0
chs_left:
    .word 0
chs_segment:
    .word 0
chs_sector:
    .byte 0

.align 4
dap:
    .byte 0x10              # Size of packet (16 bytes)
    .byte 0                 # Reserved
    .word 64                # Number of sectors to transfer (64 sectors = 32 KB)
    .word 0x0000            # Buffer Offset (0x0000) <- CHANGED TO 0
    .word 0x07e0            # Buffer Segment (0x07e0) <- CHANGED TO 0x07e0
    .quad 1                 # Starting LBA Address (Sector 1)

.org 510
.word 0xAA55
