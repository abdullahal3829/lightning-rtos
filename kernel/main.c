/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <protocol.h>
#include <stdint.h>

#define COM1 0x3F8U

/* The BIOS bootloader checks these read-only global objects before execution. */
const uint8_t EB_MAGIC = 0xAEU;
const uint32_t EB_RIVISON = 1U;

static inline void serialOut(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t serialIn(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serialInitialize(void) {
    serialOut(COM1 + 1U, 0x00U);
    serialOut(COM1 + 3U, 0x80U);
    serialOut(COM1 + 0U, 0x01U); /* 115200 baud */
    serialOut(COM1 + 1U, 0x00U);
    serialOut(COM1 + 3U, 0x03U); /* 8 data bits, no parity, one stop bit */
    serialOut(COM1 + 2U, 0xC7U);
    serialOut(COM1 + 4U, 0x0BU);
}

static void serialWrite(const char *text) {
    while (*text != '\0') {
        while ((serialIn(COM1 + 5U) & 0x20U) == 0U)
            ;
        serialOut(COM1, (uint8_t)*text++);
    }
}

void main(EBootProtocol protocol, int argc, char* argv) {
    (void)protocol;
    (void)argc;
    (void)argv;
    serialInitialize();
    serialWrite("Hello World!\r\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}