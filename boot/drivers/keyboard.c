/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include "keyboard.h"

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);

enum {
    PS2_DATA = 0x60,
    PS2_STATUS = 0x64,
    PS2_OUTPUT_FULL = 0x01,
    PS2_INPUT_FULL = 0x02,
    PS2_AUXILIARY = 0x20,
    KEYBOARD_QUEUE_SIZE = 16,
};

static const char normal_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n', [0x1E] = 'a', [0x1F] = 's',
    [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',
    [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.',
    [0x35] = '/', [0x39] = ' ',
};

static const char shifted_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x1A] = '{', [0x1B] = '}', [0x27] = ':',
    [0x28] = '"', [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>',
    [0x35] = '?',
};

static KeyboardEvent event_queue[KEYBOARD_QUEUE_SIZE];
static uint8_t queue_head;
static uint8_t queue_tail;
static uint8_t queue_count;
static uint8_t extended_prefix;
static uint8_t pause_bytes_remaining;
static uint8_t left_shift;
static uint8_t right_shift;
static uint8_t caps_lock;
static uint8_t keyboard_available;

static int waitInputBufferEmpty(void) {
    for (uint32_t attempts = 0; attempts < 100000U; ++attempts) {
        uint8_t status = inb(PS2_STATUS);
        if (status == 0xFFU)
            return 0;
        if ((status & PS2_INPUT_FULL) == 0U)
            return 1;
    }
    return 0;
}

static int waitKeyboardReply(uint8_t expected) {
    for (uint32_t attempts = 0; attempts < 100000U; ++attempts) {
        uint8_t status = inb(PS2_STATUS);
        if (status == 0xFFU)
            return 0;
        if ((status & PS2_OUTPUT_FULL) == 0U)
            continue;

        uint8_t data = inb(PS2_DATA);
        if ((status & PS2_AUXILIARY) != 0U)
            continue;
        if (data == expected)
            return 1;
        if (data == 0xFEU || data == 0xFCU || data == 0xFDU)
            return 0;
    }
    return 0;
}

static int sendKeyboardByte(uint8_t value) {
    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_DATA, value);
    return waitKeyboardReply(0xFAU);
}

int keyboard_init(void) {
    queue_head = 0U;
    queue_tail = 0U;
    queue_count = 0U;
    extended_prefix = 0U;
    pause_bytes_remaining = 0U;
    left_shift = 0U;
    right_shift = 0U;
    caps_lock = 0U;
    keyboard_available = 0U;

    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_STATUS, 0xAEU); /* Enable the first PS/2 port. */

    /* Keep the BIOS/controller-selected scan set and resume key reporting. */
    if (!sendKeyboardByte(0xF5U) || !sendKeyboardByte(0xF4U))
        return 0;

    keyboard_available = 1U;
    return 1;
}

static void enqueueEvent(uint8_t scancode, uint8_t extended, uint8_t pressed, uint8_t ascii) {
    if (queue_count == KEYBOARD_QUEUE_SIZE)
        return;

    KeyboardEvent *event = &event_queue[queue_tail];
    event->scancode = scancode;
    event->extended = extended;
    event->pressed = pressed;
    event->ascii = ascii;
    queue_tail = (uint8_t)((queue_tail + 1U) % KEYBOARD_QUEUE_SIZE);
    ++queue_count;
}

static uint8_t translateAscii(uint8_t scancode, uint8_t shifted) {
    char normal = normal_map[scancode];
    if (normal == '\0')
        return 0U;

    char shifted_char = shifted_map[scancode];
    if (normal >= 'a' && normal <= 'z') {
        if ((shifted != 0U) ^ (caps_lock != 0U))
            return (uint8_t)(normal - 'a' + 'A');
        return (uint8_t)normal;
    }
    if (shifted != 0U && shifted_char != '\0')
        return (uint8_t)shifted_char;
    return (uint8_t)normal;
}

static void processScancode(uint8_t value) {
    if (pause_bytes_remaining != 0U) {
        --pause_bytes_remaining;
        return;
    }
    if (value == 0xE1U) {
        extended_prefix = 0U;
        pause_bytes_remaining = 5U;
        return;
    }
    if (value == 0xE0U) {
        extended_prefix = 1U;
        return;
    }

    uint8_t extended = extended_prefix;
    extended_prefix = 0U;
    uint8_t pressed = (value & 0x80U) == 0U;
    uint8_t scancode = value & 0x7FU;

    if (!extended && scancode == 0x2AU) {
        left_shift = pressed;
        return;
    }
    if (!extended && scancode == 0x36U) {
        right_shift = pressed;
        return;
    }
    if (!extended && scancode == 0x3AU) {
        if (pressed)
            caps_lock ^= 1U;
        return;
    }

    uint8_t ascii = extended ? 0U : translateAscii(scancode, left_shift | right_shift);
    if (ascii != 0U || extended || scancode == 0x01U || scancode == 0x1CU ||
        scancode == 0x1DU || scancode == 0x38U ||
        scancode == 0x0FU || scancode == 0x39U)
        enqueueEvent(scancode, extended, pressed, ascii);
}

void keyboard_poll(void) {
    if (!keyboard_available)
        return;

    for (uint32_t bytes = 0; bytes < 64U; ++bytes) {
        uint8_t status = inb(PS2_STATUS);
        if (status == 0xFFU || (status & PS2_OUTPUT_FULL) == 0U)
            break;
        if ((status & PS2_AUXILIARY) != 0U)
            break; /* Leave mouse bytes for mouse_poll(). */
        processScancode(inb(PS2_DATA));
    }
}

int keyboard_read_event(KeyboardEvent *event) {
    if (event == 0 || queue_count == 0U)
        return 0;

    *event = event_queue[queue_head];
    queue_head = (uint8_t)((queue_head + 1U) % KEYBOARD_QUEUE_SIZE);
    --queue_count;
    return 1;
}

int keyboard_event_pending(void) {
    return queue_count != 0U;
}