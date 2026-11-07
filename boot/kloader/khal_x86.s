# Part Public License (PPL)
# Copyright (C) 2026 Abdullah Al Nahian/Raiyan
# View LICENSE.md

.include "protocol.h"
.include "definations.h"

# Don't Confuse With Kernel's Syscall Handler.
# This is the Primary Hardware Abstraction Layer.
# For The Kernel, this is the HAL's Syscall Handler.

.code32
.text
.global handleSyscall
.type handleSyscall, @function
handleSyscall:
    movl 4(%esp), %eax

    cmpl $SYSCALL_NDISK_READ, %eax
    je .Lsyscall_ndisk_read
    cmpl $SYSCALL_NDISK_WRITE, %eax
    je .Lsyscall_ndisk_write
    cmpl $SYSCALL_REGISTER_PSTYLE, %eax
    je .Lsyscall_register_pstyle
    cmpl $SYSCALL_GET_PARTITIONS_COUNT, %eax
    je .Lsyscall_get_partitions_count
    cmpl $SYSCALL_GET_PARTITION_ADDRESS, %eax
    je .Lsyscall_get_partition_address
    cmpl $SYSCALL_CREATE_PR_PARTITION, %eax
    je .Lsyscall_create_pr_partition
    cmpl $SYSCALL_RESIZE_PR_PARTITION, %eax
    je .Lsyscall_resize_pr_partition
    cmpl $SYSCALL_DELETE_PR_PARTITION, %eax
    je .Lsyscall_delete_pr_partition
    cmpl $SYSCALL_GET_MEMORY_MAP, %eax
    je .Lsyscall_get_memory_map
    cmpl $SYSCALL_SET_VBE_MODE, %eax
    je .Lsyscall_set_vbe_mode
    cmpl $SYSCALL_CALL16, %eax
    je .Lsyscall_call16
    cmpl $SYSCALL_CALL32, %eax
    je .Lsyscall_call32
    cmpl $SYSCALL_LIALLOCATE_MEMORY, %eax
    je .Lsyscall_liallocate_memory
    cmpl $SYSCALL_LIREALLOCATE_MEMORY, %eax
    je .Lsyscall_lireallocate_memory
    cmpl $SYSCALL_LIFREE_MEMORY, %eax
    je .Lsyscall_lifree_memory
    cmpl $SYSCALL_LIRESERVE_MEMORY, %eax
    je .Lsyscall_lireserve_memory
    cmpl $SYSCALL_LIUNRESERVE_MEMORY, %eax
    je .Lsyscall_liunreserve_memory
    cmpl $SYSCALL_SERIAL_WRITE, %eax
    je .Lsyscall_serial_write
    cmpl $SYSCALL_SERIAL_READ, %eax
    je .Lsyscall_serial_read
    cmpl $SYSCALL_SERIAL_READB, %eax
    je .Lsyscall_serial_readb
    cmpl $SYSCALL_NACPI_POWER_OFF, %eax
    je .Lsyscall_nacpi_power_off
    cmpl $SYSCALL_NACPI_REBOOT, %eax
    je .Lsyscall_nacpi_reboot
    
    movl SYSTEM_CODE_ERROR_INVALID_FUNCTION, %eax
    jmp .Lsyscall_return

.Lsyscall_ndisk_read:
    jmp .Lsyscall_return
.Lsyscall_ndisk_write:
    jmp .Lsyscall_return
.Lsyscall_register_pstyle:
    jmp .Lsyscall_return
.Lsyscall_get_partitions_count:
    jmp .Lsyscall_return
.Lsyscall_get_partition_address:
    jmp .Lsyscall_return
.Lsyscall_create_pr_partition:
    jmp .Lsyscall_return
.Lsyscall_resize_pr_partition:
    jmp .Lsyscall_return
.Lsyscall_delete_pr_partition:
    jmp .Lsyscall_return
.Lsyscall_get_memory_map:
    jmp .Lsyscall_return
.Lsyscall_set_vbe_mode:
    jmp .Lsyscall_return
.Lsyscall_call16:
    jmp .Lsyscall_return
.Lsyscall_call32:
    jmp .Lsyscall_return
.Lsyscall_liallocate_memory:
    jmp .Lsyscall_return
.Lsyscall_lireallocate_memory:
    jmp .Lsyscall_return
.Lsyscall_lifree_memory:
    jmp .Lsyscall_return
.Lsyscall_lireserve_memory:
    jmp .Lsyscall_return
.Lsyscall_liunreserve_memory:
    jmp .Lsyscall_return
.Lsyscall_serial_write:
    jmp .Lsyscall_return
.Lsyscall_serial_read:
    jmp .Lsyscall_return
.Lsyscall_serial_readb:
    jmp .Lsyscall_return
.Lsyscall_nacpi_power_off:
    jmp .Lsyscall_return
.Lsyscall_nacpi_reboot:
    jmp .Lsyscall_return

.Lsyscall_return:
    movl $42, %eax
    ret

.size handleSyscall, .-handleSyscall