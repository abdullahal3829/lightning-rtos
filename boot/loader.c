#include "vesa.h"
#include <stdint.h>

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);

enum {
    COM1 = 0x3F8,
    UART_DATA = 0,
    UART_INTERRUPT_ENABLE = 1,
    UART_LINE_CONTROL = 3,
    UART_FIFO_CONTROL = 2,
    UART_LINE_STATUS = 5,
};

static void serialInit(void) {
    /* Configure the 16550 UART before polling its transmit status. */
    outb(COM1 + UART_INTERRUPT_ENABLE, 0x00);
    outb(COM1 + UART_LINE_CONTROL, 0x80); /* Enable divisor latch. */
    outb(COM1 + UART_DATA, 0x01);         /* 115200 baud, low divisor. */
    outb(COM1 + UART_INTERRUPT_ENABLE, 0x00);
    outb(COM1 + UART_LINE_CONTROL, 0x03); /* 8 data bits, no parity, 1 stop. */
    outb(COM1 + UART_FIFO_CONTROL, 0xC7);
    outb(COM1 + 4, 0x0B);                /* DTR, RTS, and OUT2. */
}

void PSerialWrite(const char *str) {
    while (*str != '\0') {
        while ((inb(COM1 + UART_LINE_STATUS) & 0x20U) == 0U) {
            /* wait until the transmit holding register is empty */
        }
        outb(COM1 + UART_DATA, (uint8_t)*str++);
    }
}

void PSerialWriteHex(uint16_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char buf[7];

    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = digits[(value >> 12) & 0xF];
    buf[3] = digits[(value >> 8) & 0xF];
    buf[4] = digits[(value >> 4) & 0xF];
    buf[5] = digits[value & 0xF];
    buf[6] = '\0';

    PSerialWrite(buf);
}

void lmain() {
    serialInit();
    PSerialWrite("ChainLoaded\r\n");

    /* RGB blue; draw a visible 45-degree line instead of one repeated pixel. */
    for (int coordinate = 1; coordinate < 512; ++coordinate)
        drawFramebuffer(coordinate, coordinate, 0x000000FFU);

    for (;;)
        __asm__ __volatile__("hlt");
}
