/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef VARUITL_H
#define VARUITL_H

#include <stdint.h>

// Memory Utils
extern void memcpy(void* d, const void* s, size_t n);
extern void memmove(void* d, const void* s, size_t n);
extern void memset(void* d, size_t o, size_t n);
extern int32_t memcmp(const void* l, const void* r, size_t n);
extern void memnext(void* d, const void* s, size_t n);
extern void memecc(void* d, size_t n);

// String Utils
extern void sprintf(char* buffer, const char *format, ...);
extern void* usprintf(const char *format, ...); // Dynamicly Allocated sprintf function
extern int strlen(char* str);

#endif // VARUITL_H