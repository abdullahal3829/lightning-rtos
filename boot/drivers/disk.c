/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "disk.h"

enum {
    DISK_SECTOR_SIZE = 512,
    REAL_MODE_MEMORY_LIMIT = 0x100000,
};

static volatile bool spinlock;

extern int call16(void *address, void *args, size_t size);
extern uint8_t NDiskRead[];
extern uint8_t NDiskWrite[];
extern void PSerialWrite(const char *text);

typedef struct __attribute__((packed)) {
    uint16_t disk;
    uint64_t lba;
    uint16_t buffer_offset;
    uint16_t buffer_segment;
} NDiskParams;

/*
 * call16 and the BIOS require both the request and sector buffer below 1 MiB.
 *
 * NOTE: disk_bounce must be larger than a single 512-byte logical sector.  When
 * booting from CD-ROM, SeaBIOS' int 0x13 EDD handler reads a full 2048-byte CD
 * sector (four 512-byte logical sectors) into the transfer buffer regardless of
 * the sector count in the DAP.  A 512-byte bounce buffer would therefore be
 * overrun by up to 1536 bytes, spilling straight into the following .text
 * section and corrupting executable code.  4 KiB gives ample headroom for any
 * sector size the BIOS may transfer.
 */
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

    int result = 0;

    disk_params.disk = (uint16_t)disk;
    disk_params.lba = lba;
    disk_params.buffer_offset = (uint16_t)(bounce_address & 0x0FU);
    disk_params.buffer_segment = (uint16_t)(bounce_address >> 4);

    result = call16(routine, &disk_params, sizeof(disk_params));

    if (result == 0) {
        if (routine == NDiskRead)
            for (size_t i = 0; i < buffer_size; ++i)
                ((uint8_t *)buffer)[i] = disk_bounce[i];
        else
            for (size_t i = 0; i < buffer_size; ++i)
                disk_bounce[i] = ((uint8_t *)buffer)[i];
    } else {
        result = -1;
    }

    __sync_lock_release(&spinlock);
    return result;
}

int diskRead(char* diskID, uint64_t offset, uint64_t bufferSize, void *buffer) {
    return transfer((unsigned int)diskID, offset, bufferSize, buffer, NDiskRead);
}

int diskWrite(char* diskID, uint64_t offset, uint64_t bufferSize, void *buffer) {
    return transfer((unsigned int)diskID, offset, bufferSize, buffer, NDiskWrite);
}

int diskReadB(char* diskID, uint64_t offset, void *buffer) {
    return transfer((unsigned int)diskID, offset, 1, buffer, NDiskRead);
}

int diskWriteB(char* diskID, uint64_t offset, void *buffer) {
    return transfer((unsigned int)diskID, offset, 1, buffer, NDiskWrite);
}