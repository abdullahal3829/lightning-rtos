/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef VESA_H
#define VESA_H

#include <stdint.h>
#include "protocol.h"

// extern unsigned int VBEInit();
extern struct VbeInfoBlock getExtensionInfo();
extern void drawFramebuffer(int x, int y, uint64_t color);
extern uint32_t vesaGetWidth(void);
extern uint32_t vesaGetHeight(void);
extern void vesaFlush(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const uint32_t *pixels);

#endif // VESA_H