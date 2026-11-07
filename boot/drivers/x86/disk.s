# Part Public License (PPL)
# Copyright (C) 2026 Abdullah Al Nahian/Raiyan
# View LICENSE.md


.code16
.global NDiskRead
.global NDiskWrite
.global NDiskRead_end
.global NDiskWrite_end

# ============================================================
# Disk parameter structure (DS:SI)
#
# Offset  Size  Description
#   +0     2    BIOS disk number (low byte used)
#   +2     8    LBA (64-bit)
#   +10    2    Buffer offset
#   +12    2    Buffer segment
#
# Example:
#   disk_params:
#       .word 0x0080
#       .word 1
#       .word 0x8000
#       .word 0x0000
#
# Before calling:
#   movw $disk_params, %si
#   call NDiskRead
#
# Assumptions:
#   DS addresses this module's variables and parameter structure.
#
# Operations transfer exactly one sector (512 bytes).
# ============================================================


# ============================================================
# NDiskRead
# ============================================================


NDiskRead:
    call resetDiskState

    # Load parameters from DS:SI.
    movw 0(%si), %ax
    movb %al, current_disk

    movl 2(%si), %eax
    movl %eax, disk_lba
    movl 6(%si), %eax
    movl %eax, disk_lba + 4

    movw 10(%si), %ax
    movw %ax, disk_buffer

    movw 12(%si), %ax
    movw %ax, disk_segment

    # Try EDD/LBA first.
    call checkEDD
    jc .Lread_chs

    call setupDAP
    movw $disk_packet, %si
    movb current_disk, %dl
    movb $0x42, %ah
    int $0x13
    jnc .Lread_success

    # EDD failed; attempt CHS.
.Lread_chs:
    call lbaToCHS
    jc .Lread_error

    movw disk_buffer, %bx
    movw disk_segment, %ax
    movw %ax, %es

    movb chs_cylinder, %ch
    movb chs_sector, %cl
    movb chs_head, %dh
    movb current_disk, %dl

    movb $0x02, %ah
    movb $1, %al
    int $0x13
    jc .Lread_error

.Lread_success:
    xorw %ax, %ax
    clc
    lret

.Lread_error:
    movw $1, %ax
    stc
    lret
NDiskRead_end:

# ============================================================
# NDiskWrite
#
# WARNING: This overwrites the selected disk sector.
# Test using a disk image before writing to a real disk.
# ============================================================

NDiskWrite:
    call resetDiskState

    # Load parameters from DS:SI.
    movw 0(%si), %ax
    movb %al, current_disk

    movl 2(%si), %eax
    movl %eax, disk_lba
    movl 6(%si), %eax
    movl %eax, disk_lba + 4

    movw 10(%si), %ax
    movw %ax, disk_buffer

    movw 12(%si), %ax
    movw %ax, disk_segment

    # Try EDD/LBA first.
    call checkEDD
    jc .Lwrite_chs

    call setupDAP
    movw $disk_packet, %si
    movb current_disk, %dl
    movb $0x43, %ah
    movb $0, %al
    int $0x13
    jnc .Lwrite_success

    # EDD failed; attempt CHS.
.Lwrite_chs:
    call lbaToCHS
    jc .Lwrite_error

    movw disk_buffer, %bx
    movw disk_segment, %ax
    movw %ax, %es

    movb chs_cylinder, %ch
    movb chs_sector, %cl
    movb chs_head, %dh
    movb current_disk, %dl

    movb $0x03, %ah
    movb $1, %al
    int $0x13
    jc .Lwrite_error

.Lwrite_success:
    xorw %ax, %ax
    clc
    lret

.Lwrite_error:
    movw $1, %ax
    stc
    lret
NDiskWrite_end:

# ============================================================
# resetDiskState
#
# Clears all disk-related variables and the complete DAP.
# Called at the beginning of NDiskRead and NDiskWrite.
# ============================================================

resetDiskState:
    movb $0, current_disk

    movl $0, disk_lba
    movl $0, disk_lba + 4
    movw $0, disk_buffer
    movw $0, disk_segment

    movw $0, sectors_per_track
    movw $0, heads_count

    movb $0, chs_sector
    movb $0, chs_head
    movb $0, chs_cylinder

    # Reset all 16 bytes of the Disk Address Packet.
    movw $0, disk_packet
    movw $0, disk_packet + 2
    movw $0, disk_packet + 4
    movw $0, disk_packet + 6
    movl $0, disk_packet + 8
    movl $0, disk_packet + 12

    ret


# ============================================================
# checkEDD
#
# Checks whether BIOS EDD extensions are supported.
#
# Output:
#   CF = 0: EDD supported
#   CF = 1: EDD unavailable
# ============================================================

checkEDD:
    movb current_disk, %dl
    movw $0x55AA, %bx
    movb $0x41, %ah
    int $0x13
    jc .Ledd_error

    cmpw $0xAA55, %bx
    jne .Ledd_error

    testw $1, %cx
    jz .Ledd_error

    clc
    ret

.Ledd_error:
    stc
    ret


# ============================================================
# setupDAP
#
# Creates a 16-byte EDD Disk Address Packet.
#
# Transfers one sector using a 64-bit LBA field.
# The request contains a full 64-bit LBA.
# ============================================================

setupDAP:
    movb $0x10, disk_packet
    movb $0, disk_packet + 1
    movw $1, disk_packet + 2

    # Transfer buffer offset.
    movw disk_buffer, %ax
    movw %ax, disk_packet + 4

    # Transfer buffer segment.
    movw disk_segment, %ax
    movw %ax, disk_packet + 6

    movl disk_lba, %eax
    movl %eax, disk_packet + 8
    movl disk_lba + 4, %eax
    movl %eax, disk_packet + 12

    ret


# ============================================================
# lbaToCHS
#
# Converts the requested 16-bit LBA to CHS using BIOS geometry.
#
# Output:
#   chs_cylinder = low 8 bits of cylinder
#   chs_sector   = sector number plus cylinder high bits
#   chs_head     = head number
#
#   CF = 0: conversion successful
#   CF = 1: failure or unsupported cylinder
# ============================================================

lbaToCHS:
    # CHS fallback only supports a 16-bit LBA.
    cmpl $0, disk_lba + 4
    jne .Lchs_error
    cmpw $0, disk_lba + 2
    jne .Lchs_error

    # Query drive geometry.
    movb current_disk, %dl
    movb $0x08, %ah
    int $0x13
    jc .Lchs_error

    # Sectors per track are in CL bits 0-5.
    xorb %ch, %ch
    andw $0x003F, %cx
    jz .Lchs_error
    movw %cx, sectors_per_track

    # Number of heads = DH + 1.
    xorw %ax, %ax
    movb %dh, %al
    incw %ax
    movw %ax, heads_count

    # track_index = LBA / sectors_per_track
    # sector      = (LBA % sectors_per_track) + 1
    movw disk_lba, %ax
    xorw %dx, %dx
    divw sectors_per_track

    incw %dx
    movb %dl, chs_sector

    # cylinder = track_index / heads_count
    # head     = track_index % heads_count
    xorw %dx, %dx
    divw heads_count

    # CHS BIOS cylinder range is 0-1023.
    cmpw $1023, %ax
    ja .Lchs_error

    # Store head.
    movb %dl, chs_head

    # Store low 8 bits of cylinder.
    movb %al, chs_cylinder

    # Cylinder bits 8-9 go into sector byte bits 6-7.
    movb %ah, %al
    andb $0x03, %al
    shlb $6, %al
    orb chs_sector, %al
    movb %al, chs_sector

    clc
    ret

.Lchs_error:
    stc
    ret


# ============================================================
# Disk state
#
# Keep these variables in the same data segment addressed by DS.
# ============================================================

.align 4

disk_packet:
    .byte 0x10, 0
    .word 1
    .word 0              # Buffer offset
    .word 0              # Buffer segment
    .quad 0              # 64-bit LBA

current_disk:       .byte 0
chs_sector:         .byte 0
chs_head:           .byte 0
chs_cylinder:       .byte 0

.align 2

disk_lba:           .quad 0
disk_buffer:        .word 0
disk_segment:       .word 0
sectors_per_track:  .word 0
heads_count:        .word 0