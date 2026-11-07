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

extern bool isMBR((int diskRead)(uint64_t disk, uint64_t offset, void* bufr));
extern PartitionRegion getPartition((int diskRead)(uint64_t disk, uint64_t offset, void* bufr), char idx);

#endif // MBR_H