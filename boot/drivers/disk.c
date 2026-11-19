/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <definations.h>
#include "disk.h"

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
#include <Uefi.h>
#include <Protocol/BlockIo.h>
extern EFI_BOOT_SERVICES *gBS;
#else
extern int call16(void *address, void *args, size_t size);
extern uint8_t NDiskRead[];
extern uint8_t NDiskWrite[];
extern void PSerialWrite(const char *text);
#endif

enum {
    DISK_SECTOR_SIZE = 512,
    REAL_MODE_MEMORY_LIMIT = 0x100000,
};

typedef struct __attribute__((packed)) {
    uint16_t disk;
    uint64_t lba;
    uint16_t buffer_offset;
    uint16_t buffer_segment;
} NDiskParams;

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
static volatile bool spinlock;

static NDiskParams disk_params __attribute__((section(".lowdata"), aligned(16)));
static uint8_t disk_bounce[4096]
    __attribute__((section(".lowdata"), aligned(16)));

static int transfer(uint64_t disk, uint64_t lba, uint64_t buffer_size,
                    void *buffer, uint8_t *routine) {
    uintptr_t bounce_address = (uintptr_t)disk_bounce;

    if (disk > UINT8_MAX ||
        (buffer_size != 0 && buffer == NULL) ||
        buffer_size > sizeof(disk_bounce) ||
        bounce_address >= REAL_MODE_MEMORY_LIMIT ||
        buffer_size > REAL_MODE_MEMORY_LIMIT - bounce_address)
        return -1;

    while (__sync_lock_test_and_set(&spinlock, true));
    disk_params.disk = (uint16_t)disk;
    disk_params.lba = lba;
    disk_params.buffer_offset = (uint16_t)(bounce_address & 0x0FU);
    disk_params.buffer_segment = (uint16_t)(bounce_address >> 4);

    int result = call16(routine, &disk_params, sizeof(disk_params));

    if (result == 0) {
        if (routine == NDiskRead)
            for (size_t i = 0; i < buffer_size; ++i)
                ((uint8_t *)buffer)[i] = disk_bounce[i];
        else
            for (size_t i = 0; i < buffer_size; ++i)
                disk_bounce[i] = ((uint8_t *)buffer)[i];
    }

    __sync_lock_release(&spinlock);
    return result;
}
#else
static EFI_BLOCK_IO_PROTOCOL *findBlockIo(void) {
    EFI_STATUS status;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0U;
    EFI_BLOCK_IO_PROTOCOL *result = NULL;

    status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL,
                                     &handle_count, &handles);
    if (EFI_ERROR(status) || handle_count == 0U)
        return NULL;

    for (UINTN i = 0U; i < handle_count; ++i) {
        EFI_BLOCK_IO_PROTOCOL *block_io = NULL;
        status = gBS->HandleProtocol(handles[i], &gEfiBlockIoProtocolGuid, (VOID **)&block_io);
        if (!EFI_ERROR(status) && block_io != NULL && block_io->Media != NULL &&
            block_io->Media->MediaPresent) {
            result = block_io;
            break;
        }
    }

    if (handles != NULL)
        gBS->FreePool(handles);
    return result;
}

static int transfer(uint64_t disk, uint64_t lba, uint64_t buffer_size,
                        void *buffer, uint8_t is_write) {
    NDiskParams params;

    params.disk = (uint16_t)disk;
    params.lba = lba;
    params.buffer_offset = (uint16_t)((uintptr_t)buffer & 0xFFFFU);
    params.buffer_segment = (uint16_t)(((uintptr_t)buffer >> 4) & 0xFFFFU);

    if (buffer == NULL || buffer_size == 0U)
        return SYSTEM_CODE_ERROR_ILEGAL_VALUE;

    EFI_BLOCK_IO_PROTOCOL *block_io = findBlockIo();
    if (block_io == NULL || block_io->Media == NULL)
        return SYSTEM_CODE_ERROR_INVALID_BLOCK;

    UINTN block_size = (UINTN)block_io->Media->BlockSize;
    if (block_size == 0U)
        return SYSTEM_CODE_ERROR_ILEGAL_VALUE;

    EFI_LBA starting_lba = (EFI_LBA)params.lba;
    UINTN bytes = (UINTN)buffer_size;
    EFI_STATUS status;

    if (is_write != 0U)
        status = block_io->WriteBlocks(block_io, block_io->Media->MediaId,
                                       starting_lba, bytes, buffer);
    else
        status = block_io->ReadBlocks(block_io, block_io->Media->MediaId,
                                      starting_lba, bytes, buffer);

    return EFI_ERROR(status) ? SYSTEM_CODE_ERROR_GENERAL_FAILURE : 0;
}
#endif

int diskRead(char* diskID, uint64_t offset, uint64_t bufferSize, void *buffer) {
    return transfer(
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
        (unsigned int)diskID, offset, bufferSize, buffer, NDiskRead
#else
        (uint64_t)(uintptr_t)diskID, offset, bufferSize, buffer, 0U
#endif
    );
}

int diskWrite(char* diskID, uint64_t offset, uint64_t bufferSize, void *buffer) {
    return transfer(
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
        (unsigned int)diskID, offset, bufferSize, buffer, NDiskWrite
#else
        (uint64_t)(uintptr_t)diskID, offset, bufferSize, buffer, 0U
#endif
    );
}

int diskReadB(char* diskID, uint64_t offset, void *buffer) {
    return transfer(
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
        (unsigned int)diskID, offset, 1, buffer, NDiskRead
#else
        (uint64_t)(uintptr_t)diskID, offset, 1, buffer, 0U
#endif
    );
}

int diskWriteB(char* diskID, uint64_t offset, void *buffer) {
    return transfer(
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
        (unsigned int)diskID, offset, 1, buffer, NDiskWrite
#else
        (uint64_t)(uintptr_t)diskID, offset, 1, buffer, 0U
#endif
    );
}