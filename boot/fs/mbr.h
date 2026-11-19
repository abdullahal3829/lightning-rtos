/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef MBR_H
#define MBR_H

#include <stdint.h>
#include <stdbool.h>
#include <protocol.h>

#define MBR_SIGNATURE_OFFSET 0x1FEU
#define MBR_SIGNATURE 0xAA55U
#define MBR_PARTITION_TABLE_OFFSET 0x1BEU
#define MBR_PARTITION_COUNT 4U
#define MBR_PARTITION_ENTRY_SIZE 16U
#define MBR_SECTOR_SIZE 512U

typedef struct __attribute__((packed)) {
    uint8_t  bootable;          // 0x80 = bootable, 0x00 = non-bootable
    uint8_t  chs_first[3];      // CHS of first sector
    uint8_t  partition_type;    // Partition type
    uint8_t  chs_last[3];       // CHS of last sector
    uint32_t lba_first;         // LBA of first sector
    uint32_t sector_count;      // Number of sectors
} MbrPartitionEntry;

typedef struct __attribute__((packed)) {
    uint8_t                 bootstrap[446];
    MbrPartitionEntry       partitions[MBR_PARTITION_COUNT];
    uint16_t                signature;
} MbrBootSector;

bool isMBR(char *diskID);
bool mbrGetPartition(char *diskID, uint8_t idx, PartitionRegion *region);
uint8_t mbrGetPartitionCount(char *diskID);
bool mbrIsBootable(char *diskID, uint8_t idx);

#endif // MBR_H
