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
    mov sp, 0x7C00          # Stack grows downward
    cld                     # String instructions go forward
    sti                     # Re-enable interrupts
    mov [bootdrive], dl     # Save boot drive index

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
    # Fallback: Standard INT 13h AH=02h (Read Sectors CHS)
    # Floppy: LBA 1 = Cylinder 0, Head 0, Sector 2
    mov dl, [bootdrive]
    mov ah, 0x02            # Read sectors
    mov al, LOADER_SECTORS  # Number of sectors to read
    mov ch, 0               # Cylinder 0
    mov cl, 2               # Sector 2 (LBA 1 = Sector 2 on floppy)
    mov dh, 0               # Head 0
    mov bx, 0x8000          # Buffer offset (ES:BX = 0x0000:0x8000)
    int 0x13
    jc diskerr

read_ok:
    # Jump to second stage (Physical Address: 0x8000)
    mov dl, [bootdrive]
    jmp 0x0000:0x8000

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

.align 4
dap:
    .byte 0x10              # Size of packet (16 bytes)
    .byte 0                 # Reserved
    .word LOADER_SECTORS    # Number of sectors to transfer, sized automatically
    .word 0x8000            # Buffer Offset (0x8000)
    .word 0x0000            # Buffer Segment (0x0000) -> 0x0000:0x8000
    .quad 1                 # Starting LBA Address (Sector 1)

.org 510
.word 0xAA55