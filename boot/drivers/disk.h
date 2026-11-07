/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef NATIVE_DISK
#define NATIVE_DISK

#include <stdint.h>

extern int diskRead(char *diskID, uint64_t offset, uint64_t bufferSize, void *buffer);
extern int diskWrite(char *diskID, uint64_t offset, uint64_t bufferSize, void *buffer);
extern int diskReadB(char *diskID, uint64_t offset, void *buffer);
extern int diskWriteB(char *diskID, uint64_t offset, void *buffer);

#endif // NATIVE_DISK