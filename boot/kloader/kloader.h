/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef KLOADER_H
#define KLOADER_H

#include <stddef.h>
#include <stdint.h>
#include <elfparse.h>

typedef struct {
    ELFReadCallback read;
    void *context;
    uint64_t size;
} KernelFile;

typedef struct {
    uint64_t entryPoint;
    uintptr_t stackTop;
    size_t stackSize;
    uint32_t elfClass;
    uint32_t segmentCount;
} KernelImage;

extern int loadKernel(const KernelFile *file, KernelImage *image);

#endif