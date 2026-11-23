/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <protocol.h>
#include <definations.h>
#include <asm/gpioutil.h>
#include <cpu/pit.h>
#include <video/cga.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

const uint8_t EB_MAGIC = 0xAEU;
const uint32_t EB_RIVISON = 1U;

extern RunningFirmware firmware;
RunningFirmware firmware;

void INFO(const char *message, MESSAGE_TYPE type, ...) {
    if (type == SYSTEM)
        print("[SYSTEM] ", CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
    print(message, CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
    print("\n", CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
}

void WARNING(const char *message, MESSAGE_TYPE type, ...) {
    if (type == SYSTEM)
        print("[SYSTEM] ", CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
    print(message, CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
    print("\n", CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
}

void ERROR(const char *message, MESSAGE_TYPE type, ...) {
    if (type == SYSTEM)
        print("[SYSTEM] ", CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
    print(message, CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
    print("\n", CGA_COLOR_BLACK, CGA_COLOR_GREEN, false);
}

void PANIC(const char *message, MESSAGE_TYPE type, ...) {
    va_list args;
    va_start(args, message);
    ERROR(message, type, args);
    va_end(args);
    die();
}

EBootProtocol bprotocol;
void main(const EBootProtocol *protocol, int argc, char* argv) {
    (void)argc;
    (void)argv;
    INFO("Kernel booted successfully!", SYSTEM);
    if (protocol == NULL)
        PANIC("Missing boot protocol", SYSTEM);
    firmware = (RunningFirmware)protocol->firmware;
    if (firmware == BIOS && protocol->isVBEInited == false)
        cgaFillBG(CGA_COLOR_BLUE);
    else {
        char* vram = (char*)0xA000;
        /* for (int i = 0; i < splash_bmp_len; ++i)
            vram[i] = splash_bmp[i]; */
    }
    bprotocol = protocol;

    for (;;)
        __asm__ volatile ("hlt");
}