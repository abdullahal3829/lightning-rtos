/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include "mouse.h"

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);

enum {
    PS2_DATA = 0x60,
    PS2_STATUS = 0x64,
    PS2_OUTPUT_FULL = 0x01,
    PS2_INPUT_FULL = 0x02,
    PS2_AUXILIARY = 0x20,
};

static MouseState mouse_state;
static uint32_t screen_width = 800U;
static uint32_t screen_height = 600U;
static uint8_t packet[3];
static uint8_t packet_index;
static uint8_t mouse_available;

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

static int readControllerByte(uint8_t *value, int auxiliary) {
    for (uint32_t attempts = 0; attempts < 100000U; ++attempts) {
        uint8_t status = inb(PS2_STATUS);
        if (status == 0xFFU)
            return 0;
        if ((status & PS2_OUTPUT_FULL) == 0U)
            continue;

        uint8_t data = inb(PS2_DATA);
        if (((status & PS2_AUXILIARY) != 0U) != (auxiliary != 0))
            continue;
        *value = data;
        return 1;
    }
    return 0;
}

static int sendAuxByte(uint8_t value) {
    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_STATUS, 0xD4U); /* Route the next data byte to the mouse. */
    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_DATA, value);

    uint8_t reply;
    if (!readControllerByte(&reply, 1))
        return 0;
    return reply == 0xFAU;
}

int mouse_init(void) {
    mouse_available = 0U;
    packet_index = 0U;
    mouse_state.x = (int32_t)(screen_width > 24U ? screen_width - 24U : 0U);
    mouse_state.y = (int32_t)(screen_height > 24U ? screen_height - 24U : 0U);
    mouse_state.buttons = 0U;

    if (inb(PS2_STATUS) == 0xFFU || !waitInputBufferEmpty())
        return 0;

    outb(PS2_STATUS, 0xA8U); /* Enable the auxiliary PS/2 port clock. */
    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_STATUS, 0x20U); /* Read the controller command byte. */

    uint8_t command_byte;
    if (!readControllerByte(&command_byte, 0))
        return 0;

    command_byte &= (uint8_t)~0x22U; /* Polling mode; keep the aux clock enabled. */
    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_STATUS, 0x60U); /* Write the controller command byte. */
    if (!waitInputBufferEmpty())
        return 0;
    outb(PS2_DATA, command_byte);

    if (!sendAuxByte(0xF5U) || !sendAuxByte(0xF6U) || !sendAuxByte(0xF4U))
        return 0;

    mouse_available = 1U;
    return 1;
}

static void clampPosition(void) {
    if (screen_width == 0U)
        screen_width = 1U;
    if (screen_height == 0U)
        screen_height = 1U;
    if (mouse_state.x < 0)
        mouse_state.x = 0;
    if (mouse_state.y < 0)
        mouse_state.y = 0;
    if ((uint32_t)mouse_state.x >= screen_width)
        mouse_state.x = (int32_t)(screen_width - 1U);
    if ((uint32_t)mouse_state.y >= screen_height)
        mouse_state.y = (int32_t)(screen_height - 1U);
}

static void processPacket(void) {
    uint8_t flags = packet[0];
    if ((flags & 0x40U) == 0U)
        mouse_state.x += (int8_t)packet[1];
    if ((flags & 0x80U) == 0U)
        mouse_state.y -= (int8_t)packet[2];

    mouse_state.buttons = flags & 0x07U;
    clampPosition();
}

void mouse_poll(void) {
    if (!mouse_available)
        return;

    for (uint32_t bytes = 0; bytes < 64U; ++bytes) {
        uint8_t status = inb(PS2_STATUS);
        if (status == 0xFFU || (status & PS2_OUTPUT_FULL) == 0U)
            break;
        if ((status & PS2_AUXILIARY) == 0U)
            break; /* Leave keyboard bytes for keyboard_poll(). */

        uint8_t data = inb(PS2_DATA);
        if (packet_index == 0U && (data & 0x08U) == 0U)
            continue; /* Resynchronize on the packet's always-set bit 3. */
        packet[packet_index++] = data;
        if (packet_index == sizeof(packet)) {
            processPacket();
            packet_index = 0U;
        }
    }
}

void mouse_set_bounds(uint32_t width, uint32_t height) {
    screen_width = width == 0U ? 1U : width;
    screen_height = height == 0U ? 1U : height;
    mouse_state.x = (int32_t)(screen_width / 2U);
    mouse_state.y = (int32_t)(screen_height / 2U);
    clampPosition();
}

void mouse_get_state(MouseState *state) {
    if (state != 0)
        *state = mouse_state;
}