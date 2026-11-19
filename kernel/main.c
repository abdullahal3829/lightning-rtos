/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <protocol.h>
#include <definations.h>
#include <cpu/pit.h>
#include <stdint.h>
#include <stdbool.h>
#define COM1 0x3F8U

/* The BIOS bootloader checks these read-only global objects before execution. */
const uint8_t EB_MAGIC = 0xAEU;
const uint32_t EB_RIVISON = 1U;

extern RunningFirmware firmware;
RunningFirmware firmware;
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
    serialOut(COM1 + 0U, 0x01U);
    serialOut(COM1 + 1U, 0x00U);
    serialOut(COM1 + 3U, 0x03U);
    serialOut(COM1 + 2U, 0xC7U);
    serialOut(COM1 + 4U, 0x0BU);
}

static void serialWriteChar(char ch) {
    while ((serialIn(COM1 + 5U) & 0x20U) == 0U)
        ;
    serialOut(COM1, (uint8_t)ch);
}

static void serialWrite(const char *text) {
    while (*text != '\0')
        serialWriteChar(*(text++));
}

static void serialWriteU64(uint64_t value) {
    char buf[24];
    uint8_t length = 0;

    if (value == 0ULL) {
        serialWriteChar('0');
        return;
    }

    while (value != 0ULL) {
        buf[length++] = (char)('0' + (char)(value % 10ULL));
        value /= 10ULL;
    }

    while (length != 0U)
        serialWriteChar(buf[--length]);
}

void PANIC(char *message, MESSAGE_TYPE type) {
    (void)type;
    serialWrite("PANIC: ");
    serialWrite(message);
    serialWrite("\r\n");
    for (;;)
        __asm__ volatile ("cli; hlt");
}

typedef struct __attribute__((packed)) {
    uint64_t handleSyscall;
    uint64_t vbeInfo;
    uint64_t vbeModeInfo;
    uint32_t currentMode;
    uint32_t firmware;
    uint64_t imageHandle;
    EFI_SYSTEM_TABLE *SystemTable;
} KernelBootProtocol;

void main(const KernelBootProtocol *protocol, int argc, char* argv) {
    (void)argc;
    (void)argv;
    if (protocol == NULL)
        PANIC("Missing boot protocol", SYSTEM);
    firmware = (RunningFirmware)protocol->firmware;
    serialInitialize();
    serialWrite("Hello World!\r\n");

    if (firmware == UEFI && !pit_initialize(protocol->SystemTable))
        PANIC("PIT initialization failed: cannot trust kernel timebase", SYSTEM);

    if (firmware == UEFI && pit_get_frequency_hz() == 0ULL)
        PANIC("PIT frequency control failed: cannot verify runtime timebase", SYSTEM);

    for (;;)
        __asm__ volatile ("hlt");
}
