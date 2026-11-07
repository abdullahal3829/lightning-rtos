/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef VARUTIL_H
#define VARUTIL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *base;
    unsigned int sizePerBlocks;
    unsigned int count;
    unsigned int capacity;
    bool lockout;
} vector;

extern void vectorInit(vector *var);
extern int vectorAllocate(vector *var, unsigned int capacity, unsigned int blockSize);
extern int vectorAppend(vector *var, const void *base, unsigned int length);
extern void *vectorAt(vector *var, unsigned int index);
extern int vectorFind(vector *var, void* offset, void* base, unsigned int length);
extern void vectorDelete(vector *var, int index);
extern void vectorDeleteAll(vector *var);

#endif // VARUTIL_H