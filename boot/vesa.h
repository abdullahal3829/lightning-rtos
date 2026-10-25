#ifndef VESA_H
#define VESA_H

#include <stdint.h>
#include "protocol.h"

// extern unsigned int VBEInit();
extern struct VbeInfoBlock getExtensionInfo();
extern void drawFramebuffer(int x, int y, uint64_t color);

#endif // VESA_H