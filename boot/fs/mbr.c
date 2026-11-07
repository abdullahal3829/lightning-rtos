/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdint.h>
#include <stdbool.h>
#include <protocol.h>
#include "../drivers/disk.h"
#include "mbr.h"


bool isMBR((int diskRead)(uint64_t disk, uint64_t offset, void* bufr)) {
}

bool nIsMBR(char* diskID) {
}

PartitionRegion nGetPartition(char* diskID, char idx) {
    char bufr;
    if(diskReadB(diskID) != SYSTEM_CODE_SUCCESS)
        return {0};

    
}

PartitionRegion getPartition((int diskRead)(uint64_t disk, uint64_t offset, void* bufr), char idx) {
}