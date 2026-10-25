.intel_syntax noprefix
.section .text
.global _start
.code16

_start:
    lea si, [LDSuccess]
    call print
    
    cli                     # Disable interrupts during setup
    xor ax, ax
    mov ds, ax              # Data segment = 0
    mov es, ax              # Extra segment = 0
    mov ss, ax              # Stack segment = 0
    mov sp, 0x0500          # Stack grows downward
    cld                     # String instructions go forward
    sti                     # Re-enable interrupts
    mov [bootdrive], dl     # Save boot drive index

    lea si, [LDSuccess]
    call print

    # Enable A20 Line via Fast A20 Gate
    in al, 0x92
    or al, 2
    out 0x92, al

    # Try EDD Extensions (LBA) first
    mov dl, [bootdrive]
    mov ah, 0x41 	        # "check extensions present" service
    mov bx, 0x55aa
    int 0x13
    jnc edd_ok
    cmp bx, 0xaa55
    jne try_chs
    test cx, 1		        # Check support for the "fixed disk access subset"
    jz try_chs

edd_ok:
    # Read from disk using standard EDD DAP (within 1MB)
    mov dl, [bootdrive]
    lea si, [dap]           # DS:SI points to the packet
    mov ah, 0x42            # Extended Read
    int 0x13
    jnc read_ok             # Carry flag clear means success
    jmp diskerr

try_chs:
    mov dl, [bootdrive]
    
    # FIX: Point ES:BX to 0x07e0:0x0000 instead of 0x0000:0x7E00
    mov ax, 0x07e0          
    mov es, ax
    mov bx, 0x0000          
    
    mov ah, 0x02            # Read sectors function
    mov al, 64              # Number of sectors to read (32 KB)
    mov ch, 0               # Cylinder 0
    mov cl, 1               # Sector 1 (FIXED: was 2, loader.bin starts at sector 1)
    mov dh, 0               # Head 0
    int 0x13
    jc diskerr

read_ok:
    lea si, [BTSuccess]
    call print
    # Jump to second stage (Physical Address: 0x7E00)
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

BTSuccess:
    .asciz "Bootloader Swapped\r\n"

LDSuccess:
    .asciz "_start\r\n"

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