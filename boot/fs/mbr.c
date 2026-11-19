/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <definations.h>
#include <protocol.h>
#include "../drivers/disk.h"
#include "mbr.h"

bool isMBR(char *diskID) {
    uint8_t sector[MBR_SECTOR_SIZE];

    if (diskID == NULL)
        return false;

    if (diskReadB(diskID, 0, sector) != SYSTEM_CODE_SUCCESS)
        return false;

    uint16_t sig = (uint16_t)(sector[MBR_SIGNATURE_OFFSET] |
                              (uint16_t)sector[MBR_SIGNATURE_OFFSET + 1U] << 8);
    return sig == MBR_SIGNATURE;
}

bool mbrGetPartition(char *diskID, uint8_t idx, PartitionRegion *region) {
    if (region == NULL || diskID == NULL || idx >= MBR_PARTITION_COUNT)
        return false;

    uint8_t sector[MBR_SECTOR_SIZE];

    if (diskReadB(diskID, 0, sector) != SYSTEM_CODE_SUCCESS)
        return false;

    const MbrPartitionEntry *entry = (const MbrPartitionEntry *)(sector +
        MBR_PARTITION_TABLE_OFFSET + (uintptr_t)idx * MBR_PARTITION_ENTRY_SIZE);

    if (entry->partition_type == 0U)
        return false;

    region->base = (uint64_t)entry->lba_first * MBR_SECTOR_SIZE;
    region->size = (uint64_t)entry->sector_count * MBR_SECTOR_SIZE;
    return true;
}

uint8_t mbrGetPartitionCount(char *diskID) {
    uint8_t sector[MBR_SECTOR_SIZE];
    uint8_t count = 0U;

    if (diskID == NULL)
        return 0U;

    if (diskReadB(diskID, 0, sector) != SYSTEM_CODE_SUCCESS)
        return 0U;

    for (uint8_t i = 0U; i < MBR_PARTITION_COUNT; ++i) {
        const MbrPartitionEntry *entry = (const MbrPartitionEntry *)(sector +
            MBR_PARTITION_TABLE_OFFSET + (uintptr_t)i * MBR_PARTITION_ENTRY_SIZE);
        if (entry->partition_type != 0U)
            ++count;
    }

    return count;
}

bool mbrIsBootable(char *diskID, uint8_t idx) {
    if (diskID == NULL || idx >= MBR_PARTITION_COUNT)
        return false;

    uint8_t sector[MBR_SECTOR_SIZE];

    if (diskReadB(diskID, 0, sector) != SYSTEM_CODE_SUCCESS)
        return false;

    const MbrPartitionEntry *entry = (const MbrPartitionEntry *)(sector +
        MBR_PARTITION_TABLE_OFFSET + (uintptr_t)idx * MBR_PARTITION_ENTRY_SIZE);
    return entry->bootable == 0x80U;
}
