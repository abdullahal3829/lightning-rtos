/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <protocol.h>

extern void* falloc(size_t size);
extern void* fallocAt(uint64_t base, size_t size);
extern uintptr_t allocateLoaderStackTop(void);
extern void* frealloc(void* addr, size_t size);
extern CommonMemoryEntry getRegion(int index);
extern uint32_t getRegionCount(void);

#endif // PMM_H