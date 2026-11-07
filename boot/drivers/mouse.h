/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons;
} MouseState;

/* Initialize and poll the legacy i8042 PS/2 auxiliary-device interface. */
int mouse_init(void);
void mouse_poll(void);
/* Set the pointer bounds and place it at the center of the screen. */
void mouse_set_bounds(uint32_t width, uint32_t height);
void mouse_get_state(MouseState *state);

#endif // MOUSE_H