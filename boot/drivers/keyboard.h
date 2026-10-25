#ifndef BOOT_KEYBOARD_H
#define BOOT_KEYBOARD_H

#include <stdint.h>

typedef struct {
    uint8_t scancode;
    uint8_t extended;
    uint8_t pressed;
    uint8_t ascii;
} KeyboardEvent;

/* Initialize and poll the legacy i8042 PS/2 keyboard interface. */
int keyboard_init(void);
void keyboard_poll(void);
int keyboard_read_event(KeyboardEvent *event);
int keyboard_event_pending(void);

#endif
