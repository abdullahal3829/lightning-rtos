/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

# ---- Syscall codes ----
.set SYSCALL_NDISK_READ,            1
.set SYSCALL_NDISK_WRITE,           2
.set SYSCALL_REGISTER_PSTYLE,       3
.set SYSCALL_GET_PARTITIONS_COUNT,  4
.set SYSCALL_GET_PARTITION_ADDRESS, 5
.set SYSCALL_CREATE_PR_PARTITION,   6
.set SYSCALL_RESIZE_PR_PARTITION,   7
.set SYSCALL_DELETE_PR_PARTITION,   8
.set SYSCALL_GET_MEMORY_MAP,        9
.set SYSCALL_SET_VBE_MODE,          12
.set SYSCALL_CALL32,               13
.set SYSCALL_CALL16,               14
.set SYSCALL_LIALLOCATE_MEMORY,    16
.set SYSCALL_LIREALLOCATE_MEMORY,  17
.set SYSCALL_LIFREE_MEMORY,        18
.set SYSCALL_LIRESERVE_MEMORY,     19
.set SYSCALL_LIUNRESERVE_MEMORY,   20
.set SYSCALL_SERIAL_WRITE,         21
.set SYSCALL_SERIAL_READ,          22
.set SYSCALL_SERIAL_READB,         23
.set SYSCALL_NACPI_POWER_OFF,      24
.set SYSCALL_NACPI_REBOOT,         25

# ---- Error codes ----
.set SYSTEM_CODE_SUCCESS,           0
.set SYSTEM_CODE_ERROR_INVALID_FUNCTION, 1
.set SYSTEM_CODE_ERROR_READ_FAULT,  20
.set SYSTEM_CODE_ERROR_WRITE_FAULT, 19
.set SYSTEM_CODE_ERROR_GENERAL_FAILURE, 21
.set SYSTEM_CODE_ERROR_INVALID_BLOCK, 67
.set SYSTEM_CODE_ERROR_INVALID_HANDLE, 6
.set SYSTEM_CODE_ERROR_INCAPABLE,   49
.set SYSTEM_CODE_ERROR_VBE_SWITCH_FAILED, 52

.extern diskRead
.extern diskWrite
.extern diskReadB
.extern diskWriteB
.extern PSerialWrite
.extern PSerialRead
.extern PSerialReadB
.extern call16
.extern setResulation
.extern outb
.extern falloc
.extern fallocAt

# Bounce buffer for MBR reads (below 1 MiB)
.set BOUNCE_ADDR, bounce_buf

# ========================================================================
# 64-bit entry: handleSyscall64 (System V AMD64 ABI)
# BIOS calls unavailable in 64-bit mode -> INCAPABLE
# ========================================================================
.code64
.text
.global handleSyscall64
.type handleSyscall64, @function
handleSyscall64:
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    ret
.size handleSyscall64, .-handleSyscall64

# ========================================================================
# 32-bit entry: handleSyscall (cdecl)
# [esp+4]=code [esp+8]=args [esp+12]=argsSize [esp+16]=rBuffer [esp+20]=rBufferSize
# ========================================================================
.code32
.text
.global handleSyscall
.type handleSyscall, @function
handleSyscall:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %eax      # syscallCode
    movl 12(%ebp), %ebx     # args pointer
    movl 16(%ebp), %ecx     # argsSize
    movl 20(%ebp), %edx     # rBuffer pointer
    movl 24(%ebp), %esi     # rBufferSize

    cmpl $SYSCALL_NDISK_READ, %eax
    je .Lndisk_read
    cmpl $SYSCALL_NDISK_WRITE, %eax
    je .Lndisk_write
    cmpl $SYSCALL_REGISTER_PSTYLE, %eax
    je .Lincapable
    cmpl $SYSCALL_GET_PARTITIONS_COUNT, %eax
    je .Lpartitions_count
    cmpl $SYSCALL_GET_PARTITION_ADDRESS, %eax
    je .Lpartition_address
    cmpl $SYSCALL_CREATE_PR_PARTITION, %eax
    je .Lincapable
    cmpl $SYSCALL_RESIZE_PR_PARTITION, %eax
    je .Lincapable
    cmpl $SYSCALL_DELETE_PR_PARTITION, %eax
    je .Lincapable
    cmpl $SYSCALL_GET_MEMORY_MAP, %eax
    je .Lmemory_map
    cmpl $SYSCALL_SET_VBE_MODE, %eax
    je .Lset_vbe_mode
    cmpl $SYSCALL_CALL16, %eax
    je .Lcall16
    cmpl $SYSCALL_LIALLOCATE_MEMORY, %eax
    je .Lincapable
    cmpl $SYSCALL_LIREALLOCATE_MEMORY, %eax
    je .Lincapable
    cmpl $SYSCALL_LIFREE_MEMORY, %eax
    je .Lincapable
    cmpl $SYSCALL_LIRESERVE_MEMORY, %eax
    je .Lincapable
    cmpl $SYSCALL_LIUNRESERVE_MEMORY, %eax
    je .Lincapable
    cmpl $SYSCALL_SERIAL_WRITE, %eax
    je .Lserial_write
    cmpl $SYSCALL_SERIAL_READ, %eax
    je .Lserial_read
    cmpl $SYSCALL_SERIAL_READB, %eax
    je .Lserial_readb
    cmpl $SYSCALL_NACPI_POWER_OFF, %eax
    je .Lnacpi_power_off
    cmpl $SYSCALL_NACPI_REBOOT, %eax
    je .Lnacpi_reboot

    movl $SYSTEM_CODE_ERROR_INVALID_FUNCTION, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_NDISK_READ - diskRead(diskID, lba, bufferSize, buffer) from disk.c
# args: { uint64_t disk; uint64_t lba; uint64_t buffer_size; void* buffer; }
# ========================================================================
.Lndisk_read:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    movl 8(%ebp), %ebx        # args pointer
    movzwl 0(%ebx), %esi      # disk (uint16_t)
    movl 8(%ebx), %edi        # lba low
    movl 16(%ebx), %ebp       # buffer_size low
    movl 24(%ebx), %ebx       # buffer pointer

    cmpl $0xFF, %esi
    ja .Lndisk_err
    testl %ebp, %ebp
    je .Lndisk_err
    testl %ebx, %ebx
    jz .Lndisk_err
    movl %ebp, %eax
    andl $0x1FF, %eax
    jnz .Lndisk_err

    # diskRead((char*)disk, lba, bufferSize, buffer)  -- cdecl, right-to-left
    pushl %ebx                # buffer (void*)
    pushl $0                  # bufferSize high dword
    pushl %ebp                # bufferSize low dword
    pushl $0                  # lba high dword
    pushl %edi                # lba low dword
    pushl %esi                # diskID (char*, zero-extended uint16)
    call diskRead
    addl $24, %esp

    testl %eax, %eax
    jnz .Lndisk_err

    xorl %eax, %eax
    jmp .Lret

.Lndisk_err:
    movl $SYSTEM_CODE_ERROR_READ_FAULT, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_NDISK_WRITE - diskWrite(diskID, lba, bufferSize, buffer) from disk.c
# ========================================================================
.Lndisk_write:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    movl 8(%ebp), %ebx
    movzwl 0(%ebx), %esi      # disk
    movl 8(%ebx), %edi        # lba
    movl 16(%ebx), %ebp       # buffer_size
    movl 24(%ebx), %ebx       # buffer pointer

    cmpl $0xFF, %esi
    ja .Lndiskw_err
    testl %ebp, %ebp
    je .Lndiskw_err
    testl %ebx, %ebx
    jz .Lndiskw_err
    movl %ebp, %eax
    andl $0x1FF, %eax
    jnz .Lndiskw_err

    # diskWrite((char*)disk, lba, bufferSize, buffer)
    pushl %ebx
    pushl $0
    pushl %ebp
    pushl $0
    pushl %edi
    pushl %esi
    call diskWrite
    addl $24, %esp

    testl %eax, %eax
    jnz .Lndiskw_err

    xorl %eax, %eax
    jmp .Lret

.Lndiskw_err:
    movl $SYSTEM_CODE_ERROR_WRITE_FAULT, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_REGISTER_PSTYLE - unimplemented
# ========================================================================
.Lregister_pstyle:
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_GET_PARTITIONS_COUNT - diskReadB(disk 0x80, lba 0, bounce_buf)
# ========================================================================
.Lpartitions_count:
    pushl %ebx
    pushl %esi

    # diskReadB((char*)0x80, 0, bounce_buf)
    pushl $bounce_buf
    pushl $0
    pushl $0x80
    call diskReadB
    addl $12, %esp

    testl %eax, %eax
    jnz .Lpc_err

    movw bounce_buf + 0x1FE, %ax
    cmpw $0xAA55, %ax
    jne .Lpc_none

    xorl %ecx, %ecx
    xorl %esi, %esi
.Lpc_loop:
    cmpl $4, %esi
    je .Lpc_done
    cmpb $0, bounce_buf + 0x1BE + 4(%esi)
    je .Lpc_next
    incl %ecx
.Lpc_next:
    incl %esi
    jmp .Lpc_loop

.Lpc_done:
    movl %ecx, %eax
    popl %esi
    popl %ebx
    jmp .Lret

.Lpc_none:
    xorl %eax, %eax
    popl %esi
    popl %ebx
    jmp .Lret

.Lpc_err:
    movl $SYSTEM_CODE_ERROR_READ_FAULT, %eax
    popl %esi
    popl %ebx
    jmp .Lret

# ========================================================================
# SYSCALL_GET_PARTITION_ADDRESS - diskReadB then parse MBR partition entry
# ========================================================================
.Lpartition_address:
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %ebx
    movzbl 0(%ebx), %esi      # partition index

    cmpl $4, %esi
    ja .Lpa_err

    # diskReadB((char*)0x80, 0, bounce_buf)
    pushl $bounce_buf
    pushl $0
    pushl $0x80
    call diskReadB
    addl $12, %esp

    testl %eax, %eax
    jnz .Lpa_err

    movw bounce_buf + 0x1FE, %ax
    cmpw $0xAA55, %ax
    jne .Lpa_err

    movl %esi, %eax
    imull $16, %eax
    lea bounce_buf, %ebx
    addl $0x1BE, %ebx
    addl %eax, %ebx

    movzbl 4(%ebx), %eax
    testl %eax, %eax
    jz .Lpa_err

    movl 8(%ebx), %eax        # lba_first
    movl 20(%ebp), %edi       # rBuffer
    movl %eax, 0(%edi)        # lba low
    movl $0, 4(%edi)          # lba high

    movl 12(%ebx), %eax       # sector_count
    imull $512, %eax          # size bytes
    movl %eax, 8(%edi)        # size low
    movl $0, 12(%edi)         # size high

    xorl %eax, %eax
    popl %edi
    popl %esi
    popl %ebx
    jmp .Lret

.Lpa_err:
    popl %edi
    popl %esi
    popl %ebx
    movl $SYSTEM_CODE_ERROR_INVALID_BLOCK, %eax
    jmp .Lret

# ========================================================================
# Partition modification syscalls - unimplemented
# ========================================================================
.Lcreate_pr_partition:
.Lresize_pr_partition:
.Ldelete_pr_partition:
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_GET_MEMORY_MAP - call16(bios_get_mmap) (real mode only)
# ========================================================================
.Lmemory_map:
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %ebx
    movl 0(%ebx), %edi        # buffer pointer
    movl 8(%ebx), %esi        # buffer_size

    testl %edi, %edi
    jz .Lmm_err
    cmpl $24, %esi
    jb .Lmm_err

    movl %edi, %eax
    movl %eax, %ecx
    shrl $4, %eax
    movw %ax, mmap_seg
    andl $0x0FFFF, %ecx
    movw %cx, mmap_off

    movl %esi, %eax
    xorl %edx, %edx
    movl $24, %ebx
    idivl %ebx
    movw %ax, mmap_max

    subl $6, %esp
    movl %esp, %edi

    movw mmap_seg, %ax
    movw %ax, 0(%edi)
    movw mmap_off, %ax
    movw %ax, 2(%edi)
    movw mmap_max, %ax
    movw %ax, 4(%edi)

    pushl $6
    pushl %edi
    pushl $bios_get_mmap
    call call16
    addl $12, %esp
    addl $6, %esp

    testw %ax, %ax
    jnz .Lmm_err

    movl 20(%ebp), %edi
    movw %ax, 0(%edi)
    movl $0, 4(%edi)

    xorl %eax, %eax
    popl %edi
    popl %esi
    popl %ebx
    jmp .Lret

.Lmm_err:
    popl %edi
    popl %esi
    popl %ebx
    movl $SYSTEM_CODE_ERROR_GENERAL_FAILURE, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_SET_VBE_MODE - call16(bios_set_vbe_mode) (real mode only)
# ========================================================================
.Lset_vbe_mode:
    pushl %ebx

    movl 8(%ebp), %ebx
    movzwl 0(%ebx), %eax

    subl $4, %esp
    movl %esp, %edi
    movw %ax, 0(%edi)

    pushl $2
    pushl %edi
    pushl $setResulation
    call call16
    addl $12, %esp

    testw %ax, %ax
    jnz .Lsvm_err

    xorl %eax, %eax
    popl %ebx
    jmp .Lret

.Lsvm_err:
    popl %ebx
    movl $SYSTEM_CODE_ERROR_VBE_SWITCH_FAILED, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_CALL16 - call16(flat_addr, args_ptr, args_size)
# ========================================================================
.Lcall16:
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %ebx
    movzwl 0(%ebx), %edx
    movzwl 2(%ebx), %ecx
    movl 4(%ebx), %esi
    movzwl 8(%ebx), %edi

    movl %edx, %eax
    shll $4, %eax
    addl %ecx, %eax

    cmpl $0xFFFFF, %eax
    ja .Lcall16_err

    pushl %edi
    pushl %esi
    pushl %eax
    call call16
    addl $12, %esp

    popl %edi
    popl %esi
    popl %ebx
    jmp .Lret

.Lcall16_err:
    popl %edi
    popl %esi
    popl %ebx
    movl $SYSTEM_CODE_ERROR_INVALID_HANDLE, %eax
    jmp .Lret

# ========================================================================
# Memory management syscalls - linear allocator wrappers around mm.c
# ========================================================================
.Lliallocate_memory:
    # args: { uint64_t size; }
    pushl %ebx
    pushl %esi

    movl 8(%ebp), %ebx        # args pointer
    movl 0(%ebx), %eax        # size low (uint64_t)

    testl %eax, %eax
    jz .Lli_fail

    pushl %eax
    call linear_alloc
    addl $4, %esp

    popl %esi
    popl %ebx
    jmp .Lret                 # eax = allocated address

.Llireallocate_memory:
    # Linear allocator does not support realloc
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    jmp .Lret

.Lsys_lifree_memory:
    # Linear allocator: free is no-op (reset via linear_reset instead)
    xorl %eax, %eax
    jmp .Lret

.Lli_reserve_memory:
    # args: { uint64_t base; uint64_t size; }
    pushl %ebx
    pushl %esi

    movl 8(%ebp), %ebx        # args pointer
    movl 0(%ebx), %eax        # base low
    movl 8(%ebx), %ecx        # size low

    testl %eax, %eax
    jz .Lli_fail
    testl %ecx, %ecx
    jz .Lli_fail

    pushl %ecx                # size
    pushl %eax                # base
    call fallocAt
    addl $8, %esp

    popl %esi
    popl %ebx
    jmp .Lret                 # eax = reserved address or 0

.Lli_unreserve_memory:
    # Linear allocator does not support individual unreserve
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    jmp .Lret

.Lli_fail:
    xorl %eax, %eax
    popl %esi
    popl %ebx
    jmp .Lret

# ========================================================================
# SYSCALL_SERIAL_WRITE - PSerialWrite(text, length) from loader.c
# ========================================================================
.Lserial_write:
    pushl %ebx
    pushl %esi

    movl 8(%ebp), %ebx
    movl 0(%ebx), %esi
    movl 8(%ebx), %edi

    testl %esi, %esi
    jz .Lserw_err
    testl %edi, %edi
    je .Lserw_done

    pushl $0
    pushl %edi
    pushl %esi
    call PSerialWrite
    addl $12, %esp

.Lserw_done:
    xorl %eax, %eax
    popl %esi
    popl %ebx
    jmp .Lret

.Lserw_err:
    popl %esi
    popl %ebx
    movl $SYSTEM_CODE_ERROR_WRITE_FAULT, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_SERIAL_READ - PSerialRead(buffer, length) from loader.c
# ========================================================================
.Lserial_read:
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 8(%ebp), %ebx
    movl 0(%ebx), %esi
    movl 8(%ebx), %edi

    testl %esi, %esi
    jz .Lserr_err
    testl %edi, %edi
    je .Lserr_done

    pushl $0
    pushl %edi
    pushl %esi
    call PSerialRead
    addl $12, %esp

.Lserr_done:
    xorl %eax, %eax
    popl %edi
    popl %esi
    popl %ebx
    jmp .Lret

.Lserr_err:
    popl %edi
    popl %esi
    popl %ebx
    xorl %eax, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_SERIAL_READB - PSerialReadB() from loader.c
# ========================================================================
.Lserial_readb:
    call PSerialReadB
    movzwl %ax, %eax
    jmp .Lret

# ========================================================================
# SYSCALL_NACPI_POWER_OFF - call16(bios_apm_call) (real mode only)
# ========================================================================
.Lnacpi_power_off:
    subl $6, %esp
    movw $0x5307, 0(%esp)
    movw $0x0001, 2(%esp)
    movw $0x0003, 4(%esp)
    pushl $6
    pushl %esp
    pushl $bios_apm_call
    call call16
    addl $12, %esp
    addl $6, %esp
    cli
    hlt
    jmp .Lnacpi_power_off

# ========================================================================
# SYSCALL_NACPI_REBOOT - call16(bios_apm_call) then keyboard controller
# ========================================================================
.Lnacpi_reboot:
    subl $6, %esp
    movw $0x5307, 0(%esp)
    movw $0x0001, 2(%esp)
    movw $0x0002, 4(%esp)
    pushl $6
    pushl %esp
    pushl $bios_apm_call
    call call16
    addl $12, %esp
    addl $6, %esp

    movb $0xFE, %al
    outb %al, $0x64

    cli
    hlt
    jmp .Lnacpi_reboot

# ---- Common return ----
.Lincapable:
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    jmp .Lret

.Lret:
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

# ========================================================================
# Linear Allocator - bump pointer wrapper around mm.c falloc/fallocAt
#
# Mission critical design:
# - O(1) bump pointer allocation
# - 4-byte alignment for safety
# - Chunk-based: when current chunk exhausted, calls falloc() for new chunk
# - No individual free (linear allocator semantics)
# - linear_reset() for phase-based deallocation
# ========================================================================

# ========================================================================
# linear_init(size) - Initialize bump allocator with initial region
#   Calls falloc() from mm.c to get initial chunk
# args: { uint64_t size; }
# Returns: eax = end address of region (0 on failure)
# ========================================================================
linear_init:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx

    movl 8(%ebp), %eax        # initial_size
    testl %eax, %eax
    jz .li_init_fail

    # Call falloc(size) - mm.c handles 4KB alignment internally
    pushl %eax
    call falloc
    addl $4, %esp

    testl %eax, %eax
    jz .li_init_fail

    movl %eax, %ebx           # ebx = base address
    movl %ebx, linear_bump    # bump = base
    movl %ebx, linear_base    # save base for reset

    movl 8(%ebp), %eax        # size
    addl %eax, %ebx           # ebx = base + size = end
    movl %ebx, linear_end     # end = base + size

    movl $1, linear_active    # mark initialized

    movl %ebx, %eax           # return end address
    popl %ebx
    popl %ebp
    ret

.li_init_fail:
    movl $0, linear_active
    xorl %eax, %eax
    popl %ebx
    popl %ebp
    ret

# ========================================================================
# linear_alloc(size) - Bump pointer allocation
#   Aligns size to 4 bytes, bumps pointer, returns old bump
#   If current chunk exhausted, calls falloc() for new chunk
# args: { uint64_t size; }
# Returns: eax = allocated address (0 on failure)
# ========================================================================
linear_alloc:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi

    cmpl $1, linear_active
    jne .li_alloc_fail        # not initialized

    movl 8(%ebp), %eax        # size
    testl %eax, %eax
    jz .li_alloc_fail

    # Align size to 4 bytes
    addl $3, %eax
    andl $~3, %eax

    # Check if we have room in current chunk
    movl linear_bump, %ebx    # ebx = current bump
    movl linear_end, %esi     # esi = end
    addl %eax, %ebx           # ebx = new bump (candidate)
    cmpl %esi, %ebx           # compare new_bump vs end
    ja .li_alloc_new_chunk    # if new_bump > end, need new chunk

    # We have room - allocate from current chunk
    movl linear_bump, %eax    # eax = return address (old bump)
    movl %ebx, linear_bump    # update bump

    popl %esi
    popl %ebx
    popl %ebp
    ret

.li_alloc_new_chunk:
    # Current chunk exhausted - call falloc() for new chunk
    movl 8(%ebp), %eax        # original size

    pushl %eax
    call falloc
    addl $4, %esp

    testl %eax, %eax
    jz .li_alloc_fail

    # New chunk allocated at %eax
    movl %eax, %ebx           # ebx = new base
    movl %eax, linear_bump    # bump = base
    movl %eax, linear_base    # update base too

    movl 8(%ebp), %eax        # size
    addl %eax, %ebx           # ebx = base + size
    movl %ebx, linear_end     # end

    movl %eax, %eax           # return address already in eax

    popl %esi
    popl %ebx
    popl %ebp
    ret

.li_alloc_fail:
    xorl %eax, %eax
    popl %esi
    popl %ebx
    popl %ebp
    ret

# ========================================================================
# linear_reserve(base, size) - Reserve specific address via fallocAt()
#   Wrapper around mm.c fallocAt()
# args: { uint64_t base; uint64_t size; }
# Returns: eax = base address (0 on failure)
# ========================================================================
linear_reserve:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx

    movl 8(%ebp), %eax        # base low
    movl 16(%ebp), %ecx       # size

    testl %eax, %eax
    jz .li_reserve_fail
    testl %ecx, %ecx
    jz .li_reserve_fail

    # fallocAt(base, size) - cdecl, right-to-left
    pushl %ecx                # size
    pushl %eax                # base
    call fallocAt
    addl $8, %esp

    testl %eax, %eax
    jz .li_reserve_fail

    popl %ebx
    popl %ebp
    ret

.li_reserve_fail:
    xorl %eax, %eax
    popl %ebx
    popl %ebp
    ret

# ========================================================================
# linear_reset() - Reset bump pointer to region start
#   For phase-based deallocation (free all allocations since init)
# Returns: eax = region base address (0 if not initialized)
# ========================================================================
linear_reset:
    cmpl $1, linear_active
    jne .li_reset_fail

    movl linear_base, %eax    # return base address
    movl %eax, linear_bump    # bump = base

    xorl %edx, %edx           # success (return base in eax)
    ret

.li_reset_fail:
    movl $SYSTEM_CODE_ERROR_INCAPABLE, %eax
    ret

# ========================================================================
# 16-bit BIOS routines (called via call16) - real mode only
# NOT duplicated from loader.c or loader.s
# ========================================================================

.code16
.text

bios_get_mmap:
    pushw %ds
    pushw %es
    pushw %di
    pushw %si

    movw 0(%si), %ax
    movw %ax, %es
    movw 2(%si), %di
    movw 4(%si), %cx

    xorl %ebx, %ebx
    xorl %ebp, %ebp

.bmm_loop:
    cmpl %ecx, %ebp
    jae .bmm_done

    movl $1, 32(%edi)

    movl $0xE820, %eax
    movl $0x534D4150, %edx
    movl $24, %ecx
    int $0x15
    jc .bmm_err

    cmpl $0x534D4150, %eax
    jne .bmm_err

    testl %ebx, %ebx
    jz .bmm_done

    cmpl $20, %ecx
    jb .bmm_err

    addl $24, %edi
    incl %ebp

    testb $0, %bl
    jne .bmm_loop

.bmm_done:
    movw %bp, %ax
    clc
    jmp .bmm_ret

.bmm_err:
    xorw %ax, %ax
    stc

.bmm_ret:
    popw %si
    popw %di
    popw %es
    popw %ds
    ret

bios_apm_call:
    pushw %ds
    pushw %si

    movw 0(%si), %ax
    movw 2(%si), %bx
    movw 4(%si), %cx
    movw $0x5307, %ax
    int $0x15
    jc .bapm_err

    popw %si
    popw %ds
    ret

.bapm_err:
    popw %si
    popw %ds
    movw $0xFFFF, %ax
    ret

# ========================================================================
# Data (below 1 MiB, real-mode accessible)
# ========================================================================
.section .data
.balign 16
bounce_buf:
    .space 512
.balign 4
mmap_seg:
    .word 0
.balign 4
mmap_off:
    .word 0
.balign 4
mmap_max:
    .word 0
.balign 4
linear_base:    .long 0       # linear allocator region base
linear_bump:    .long 0       # current allocation pointer
linear_end:     .long 0       # end of current region
linear_active:  .long 0       # initialized flag