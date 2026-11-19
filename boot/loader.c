/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/SerialIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#endif

#include "vesa.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "lvgl.h"
#include "fs/toritofs.h"
#include "kloader/kloader.h"
#include "mm.h"
#include <definations.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern uint8_t bootdrive;
extern int call16(void *address, void *args, size_t size);
extern uint8_t printTL16[];
extern uint8_t setTextMode16[];
extern void NDiskRead(void);
#endif

extern void die(void);
extern void prepareLongMode(uint64_t entryPoint, uintptr_t stackTop,
                            const void *bootProtocol);
extern uint32_t handleSyscall64(uint32_t syscallCode, uint64_t args,
                                uint64_t argsSize, uint64_t rBuffer,
                                uint64_t rBufferSize);
bool secureMode = false;
bool ancientTheme = false;
/* Set by the real-mode video setup when no linear framebuffer exists.  The
 * .lowdata section keeps it out of the BSS range that lmain() clears. */
bool headlessMode __attribute__((section(".lowdata"))) = false;

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
extern EFI_SYSTEM_TABLE *gST;
extern EFI_HANDLE gImageHandle;
static EFI_SERIAL_IO_PROTOCOL *g_serial_io = NULL;
#endif

void PSerialRead(char *buffer, uint64_t length) {
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    UINTN n = (UINTN)length;
    if (g_serial_io == NULL || buffer == NULL)
        return;
    g_serial_io->Read(g_serial_io, &n, buffer);
#endif
}

uint8_t PSerialReadB(void) {
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    UINTN n = 1;
    uint8_t ch = 0;
    if (g_serial_io == NULL)
        return 0;
    if (g_serial_io->Read(g_serial_io, &n, &ch) == EFI_SUCCESS && n == 1U)
        return ch;
    return 0;
#else
    return 0;
#endif
}

#ifndef UEFI_BUILD
extern int call16(void *address, void *args, size_t size);
extern uint8_t printTL16[];
extern uint8_t setTextMode16[];
#endif
void PSerialWrite(const char *str);
static void loadKernelBootEntry(void);

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
static void printTL(const char *text) {
    Print(L"%a", text);
}
#else
static void printTL(const char *text) {
    size_t length = 0;
    while (text[length] != '\0')
        ++length;
    (void)call16((void *)printTL16, (void *)text, length + 1U);
}
#endif

static void debugInvalidationState(const char *stage, lv_display_t *display) {
    PSerialWrite(stage);
    PSerialWrite(lv_display_is_invalidation_enabled(display) ?
                 " invalidation=enabled\r\n" : " invalidation=disabled\r\n");
}

enum {
    SERIAL_PORT_TABLE = 0x0400,
    SERIAL_PORT_COUNT = 4,
    UART_DATA = 0,
    UART_INTERRUPT_ENABLE = 1,
    UART_LINE_CONTROL = 3,
    UART_FIFO_CONTROL = 2,
    UART_LINE_STATUS = 5,
    UART_TX_READY = 0x20,
};

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
static uint16_t serialPortAt(uint32_t index) {
    volatile uint16_t *ports = (volatile uint16_t *)(uintptr_t)SERIAL_PORT_TABLE;
    return ports[index];
}

static int serialPortPresent(uint16_t port) {
    return port != 0U && inb(port + UART_LINE_STATUS) != 0xFFU;
}

static int waitForTransmitReady(uint16_t port) {
    for (uint32_t attempts = 0; attempts < 100000U; ++attempts) {
        uint8_t status = inb(port + UART_LINE_STATUS);
        if (status == 0xFFU)
            return 0;
        if ((status & UART_TX_READY) != 0U)
            return 1;
    }
    return 0;
}

static void serialInit(void) {
    /* Configure COM1 (0x3F8) unconditionally.  The BIOS data area may not
     * report the UART as present until after a BIOS disk call (int 0x13) has
     * been made, so we cannot rely on serialPortPresent() here.  Configure
     * the port directly; PSerialWrite() will check serialPortPresent() on
     * each write. */
    uint16_t port = 0x3F8;
    outb(port + UART_INTERRUPT_ENABLE, 0x00);
    outb(port + UART_LINE_CONTROL, 0x80); /* Enable divisor latch. */
    outb(port + UART_DATA, 0x01);         /* Divisor low byte (115200 baud). */
    outb(port + UART_INTERRUPT_ENABLE, 0x00);
    outb(port + UART_LINE_CONTROL, 0x03); /* 8 data bits, no parity, 1 stop. */
    outb(port + UART_FIFO_CONTROL, 0xC7);
    outb(port + 4, 0x0B);                /* DTR, RTS, and OUT2. */
    /* Record COM1 in the BIOS Data Area so PSerialWrite() can discover it. */
    *(volatile uint16_t *)(uintptr_t)0x0400 = port;
}

void PSerialWrite(const char *str) {
    for (uint32_t i = 0; i < SERIAL_PORT_COUNT; ++i) {
        uint16_t port = serialPortAt(i);
        if (!serialPortPresent(port))
            continue;

        const char *current = str;
        while (*current != '\0') {
            if (!waitForTransmitReady(port))
                break;
            outb(port + UART_DATA, (uint8_t)*current++);
        }
    }
}
#else
static void serialInit(void) {
    /* OVMF/QEMU does not reliably expose a working SerialIo protocol under
     * -nographic -serial.  The kernel initializes COM1 port I/O on its own,
     * so the bootloader does not need firmware serial services.  Leave
     * g_serial_io NULL so PSerialWrite() is a safe no-op. */
    g_serial_io = NULL;
}

void PSerialWrite(const char *str) {
    UINTN length;
    const char *current;

    if (g_serial_io == NULL || str == NULL)
        return;

    length = 0;
    current = str;
    while (*current != '\0') {
        ++length;
        ++current;
    }

    g_serial_io->Write(g_serial_io, &length, (void *)str);
}
#endif

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

extern uint8_t __bss_start[];
extern uint8_t __bss_end[];
#ifndef UEFI_BUILD
extern VbeInfoBlock g_vbe_info;
extern struct vbe_mode_info_structure g_vbe_mode_info;
extern int vbeIsInited;
extern uint16_t curVBEMode;
#endif

static uint32_t lvgl_draw_buffer[800U * 24U];
static bool cursor_visible;
static bool mouse_position_initialized;
static int32_t last_mouse_x;
static int32_t last_mouse_y;
static uint32_t last_mouse_activity;
static lv_display_t *boot_display;
static lv_indev_t *boot_mouse;
static lv_indev_t *boot_keyboard;
static bool ctrl_down;
static bool alt_down;
static void reboot_btn_cb(lv_event_t *event);


#if !(defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))
static void haltForCtrlAltDelete(void) {
    for (;;);
}
#endif

static void handleKeyboardHotkey(const KeyboardEvent *event) {
    if (event->scancode == 0x1DU)
        ctrl_down = event->pressed;
    else if (event->scancode == 0x38U)
        alt_down = event->pressed;
#if defined(__i386__) || defined(_M_IX86)
    else if (event->extended && event->scancode == 0x53U && event->pressed &&
             ctrl_down && alt_down)
        reboot_btn_cb(NULL);
#elif !(defined(__x86_64__) || defined(_M_X64))
    else if (event->extended && event->scancode == 0x53U && event->pressed &&
             ctrl_down && alt_down)
        haltForCtrlAltDelete();
#endif
}

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
static uint16_t readPitCounter(void) {
    outb(0x43U, 0x00U); /* Latch channel 0's current countdown value. */
    uint8_t low = inb(0x40U);
    uint8_t high = inb(0x40U);
    return (uint16_t)(((uint16_t)high << 8) | low);
}
#else
static uint16_t readPitCounter(void) {
    return 0U;
}
#endif

static uint32_t keyboardEventToLvglKey(const KeyboardEvent *event) {
#ifndef UEFI_BUILD
    if (event->extended) {
        switch (event->scancode) {
            case 0x48: return LV_KEY_PREV;
            case 0x50: return LV_KEY_NEXT;
            case 0x4B: return LV_KEY_LEFT;
            case 0x4D: return LV_KEY_RIGHT;
            case 0x47: return LV_KEY_HOME;
            case 0x4F: return LV_KEY_END;
            case 0x53: return LV_KEY_DEL;
            default: return 0U;
        }
    }

    switch (event->scancode) {
        case 0x01: return LV_KEY_ESC;
        case 0x0E: return LV_KEY_BACKSPACE;
        case 0x0F: return LV_KEY_NEXT;
        case 0x1C: return LV_KEY_ENTER;
        default: return event->ascii;
    }
#else
    // Implement Returning Keycode For UEFI When Press Key
#endif
}

static void keyboardRead(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    keyboard_poll();
    KeyboardEvent event;
    while (keyboard_read_event(&event)) {
        handleKeyboardHotkey(&event);
        uint32_t key = keyboardEventToLvglKey(&event);
        if (key == 0U)
            continue;
        data->key = key;
        data->state = event.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = keyboard_event_pending();
        return;
    }
    data->key = 0U;
    data->state = LV_INDEV_STATE_RELEASED;
}

static void mouseRead(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    mouse_poll();
    MouseState state;
    mouse_get_state(&state);
    if (!mouse_position_initialized) {
        last_mouse_x = state.x;
        last_mouse_y = state.y;
        mouse_position_initialized = true;
    } else if (state.x != last_mouse_x || state.y != last_mouse_y) {
        last_mouse_x = state.x;
        last_mouse_y = state.y;
        last_mouse_activity = lv_tick_get();
        cursor_visible = true;
        lv_obj_set_hidden(lv_indev_get_cursor(indev), false);
    } else if (cursor_visible && (uint32_t)(lv_tick_get() - last_mouse_activity) >= 5000U) {
        cursor_visible = false;
        lv_obj_set_hidden(lv_indev_get_cursor(indev), true);
    }
    data->point.x = (lv_coord_t)state.x;
    data->point.y = (lv_coord_t)state.y;
    data->state = (state.buttons & 0x01U) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
static void clearBssAndPreserveVideoState(void) {
    VbeInfoBlock saved_info;
    uint8_t *saved_bytes = (uint8_t *)&saved_info;
    const uint8_t *current_bytes = (const uint8_t *)&g_vbe_info;
    for (uint32_t i = 0; i < sizeof(saved_info); ++i)
        saved_bytes[i] = current_bytes[i];

    int saved_vbe_state = vbeIsInited;
    uint16_t saved_mode = curVBEMode;
    for (uint8_t *byte = __bss_start; byte < __bss_end; ++byte)
        *byte = 0;

    uint8_t *restored_bytes = (uint8_t *)&g_vbe_info;
    for (uint32_t i = 0; i < sizeof(saved_info); ++i)
        restored_bytes[i] = saved_bytes[i];
    vbeIsInited = saved_vbe_state;
    curVBEMode = saved_mode;
}
#else
static void clearBssAndPreserveVideoState(void) {
    for (uint8_t *byte = __bss_start; byte < __bss_end; ++byte)
        *byte = 0;
}
#endif

static void flushDisplay(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    static bool flush_logged;
    uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
    if (!flush_logged) {
        flush_logged = true;
        PSerialWrite("Framebuffer flush callback reached.\r\n");
    }
#ifndef UEFI_BUILD
    vesaFlush((uint32_t)area->x1, (uint32_t)area->y1, width, height, (const uint32_t *)pixels);
#endif
    lv_display_flush_ready(display);
}

static lv_obj_t *makePanel(lv_obj_t *parent, int x, int y, int width, int height,
                           uint32_t fill, uint32_t outline, int radius) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(fill), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(outline), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    return panel;
}

static lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y,
                           const lv_font_t *font, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    return label;
}

static lv_obj_t *selectedBootEntry;
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
typedef struct {
    EFI_FILE_PROTOCOL *file;
    UINT64 size;
} UefiFileContext;

static int uefiReadFile(void *context, uint64_t offset, void *buffer, size_t size);
static int uefiOpenFile(const char *path, UefiFileContext *ufc);
#endif
static void loadKernelBootEntry(void) {
    ToritoFile source;
    KernelFile kernel = {
        .read = toritoReadFile,
        .context = &source,
        .size = 0U
    };
    KernelImage image;
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    UefiFileContext ufc = {NULL, 0};
#endif
    PSerialWrite("Loading boot entry /oskrnl.\r\n");
    PSerialWrite("Boot drive: ");
#ifndef UEFI_BUILD
    PSerialWriteHex((uint16_t)bootdrive);
#endif
    PSerialWrite("\r\n");
    /* Direct COM1 debug: confirm we reached toritoOpenFile */
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'T'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'O'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'F'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    kernel.read = uefiReadFile;
    kernel.context = &ufc;
    int result = uefiOpenFile("/oskrnl", &ufc);
    if (result == SYSTEM_CODE_SUCCESS && ufc.file == NULL)
        result = SYSTEM_CODE_ERROR_INVALID_HANDLE;
    if (result == SYSTEM_CODE_SUCCESS)
        kernel.size = ufc.size;
#else
    int result = toritoOpenFile(bootdrive, "/oskrnl", &source);
    /* Direct COM1 debug: show toritoOpenFile result */
    {
        char msg[] = "R=";
        for (uint32_t _t = 0; _t < 2; ++_t)
            for (uint32_t _w = 0; _w < 100000U; ++_w)
                if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, msg[_t]); break; }
        {
            static const char digits[] = "0123456789ABCDEF";
            uint32_t v = (uint32_t)result;
            for (int _n = 3; _n >= 0; --_n) {
                char c = digits[(v >> (_n * 4)) & 0xF];
                for (uint32_t _w = 0; _w < 100000U; ++_w)
                    if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, c); break; }
            }
        }
        for (uint32_t _w = 0; _w < 100000U; ++_w)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
        for (uint32_t _w = 0; _w < 100000U; ++_w)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
    }
    if (result == SYSTEM_CODE_SUCCESS)
        kernel.size = source.size;
#endif
    /* Direct COM1 debug: branch on result */
    if (result == 0U) {
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'O'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'K'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
    } else {
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'F'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'A'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'I'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'L'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
    }
    /* Test: direct call16(NDiskRead) to read sector 16 (ISO PV descriptor) */
    {
        struct NDiskTestParams {
            uint16_t disk;
            uint64_t lba;
            uint16_t buffer_offset;
            uint16_t buffer_segment;
        } __attribute__((packed)) params;
        static uint8_t test_buf[2048] __attribute__((section(".lowdata")));

        params.disk = bootdrive;
        params.lba = 16;
        params.buffer_offset = (uint16_t)((uintptr_t)test_buf & 0xFFFF);
        params.buffer_segment = (uint16_t)((uintptr_t)test_buf >> 4);

        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'S'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '1'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '6'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }

        int cres = call16((void *)NDiskRead, &params, sizeof(params));

        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'R'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '='); break; }
        {
            static const char digits[] = "0123456789ABCDEF";
            uint32_t v = (uint32_t)(uint16_t)cres;
            for (int _n = 3; _n >= 0; --_n) {
                char c = digits[(v >> (_n * 4)) & 0xF];
                for (uint32_t _w = 0; _w < 100000U; ++_w)
                    if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, c); break; }
            }
        }
        for (uint32_t _w = 0; _w < 100000U; ++_w)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
        for (uint32_t _w = 0; _w < 100000U; ++_w)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }

        /* Check first byte of PV descriptor (should be 0x01 = boot record) */
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'F'); break; }
        for (uint32_t _t = 0; _t < 100000U; ++_t)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'B'); break; }
        {
            static const char digits[] = "0123456789ABCDEF";
            uint32_t v = test_buf[0];
            for (int _n = 1; _n >= 0; --_n) {
                char c = digits[(v >> (_n * 4)) & 0xF];
                for (uint32_t _w = 0; _w < 100000U; ++_w)
                    if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, c); break; }
            }
        }
        for (uint32_t _w = 0; _w < 100000U; ++_w)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
        for (uint32_t _w = 0; _w < 100000U; ++_w)
            if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
    }
    if (result == SYSTEM_CODE_ERROR_FILE_NOT_FOUND) {
        PSerialWrite("Kernel file /oskrnl was not found on the boot media.\r\n");
        PANIC("A Specified File Not Found");
        return;
    }
    if (result != SYSTEM_CODE_SUCCESS) {
        PSerialWrite("Opening /oskrnl failed with code ");
        PSerialWriteHex((uint16_t)result);
        PSerialWrite("\r\n");
        /* Spin briefly so the error code is visible before any triple fault. */
        for (volatile uint32_t _delay = 0; _delay < 20000000U; _delay++)
            __asm__ volatile ("nop");
        PANIC("Invalid Kernel");
        return;
    }
    serialInit();
    /* Direct COM1 debug: confirm we reached loadKernel */
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'K'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'E'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'L'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
    /* Direct COM1 debug: confirm loadKernel is called */
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'G'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, 'O'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\r'); break; }
    for (uint32_t _t = 0; _t < 100000U; ++_t)
        if ((inb(0x3FDU) & 0x20U) != 0U) { outb(0x3F8U, '\n'); break; }
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
    kernel.size = source.size;
#endif
    result = loadKernel(&kernel, &image);
    if (result != 0) {
        PSerialWrite("ELF loader failed with code ");
        PSerialWriteHex((uint16_t)result);
        PSerialWrite("\r\n");
        PANIC("Invalid Kernel");
        return;
    }
    if (image.elfClass != 2U) {
        PSerialWrite("Kernel is not a 64-bit ELF image.\r\n");
        PANIC("Invalid Kernel");
        return;
    }

    struct __attribute__((packed)) KernelBootProtocol64 {
        uint64_t handleSyscall;
        uint64_t vbeInfo;
        uint64_t vbeModeInfo;
        uint32_t currentMode;
        uint32_t firmware;
        uint64_t imageHandle;
        uint64_t systemTable;
    } protocol = {
        .handleSyscall = (uint64_t)(uintptr_t)handleSyscall64,
#ifndef UEFI_BUILD
        .vbeInfo = (uint64_t)(uintptr_t)&g_vbe_info,
        .vbeModeInfo = (uint64_t)(uintptr_t)&g_vbe_mode_info,
        .currentMode = (uint32_t)curVBEMode,
        .firmware = BIOS, /* BIOS */
#else
        .firmware = UEFI,
        .imageHandle = (uint64_t)(uintptr_t)gImageHandle,
        .systemTable = (uint64_t)(uintptr_t)gST
#endif
    };

    PSerialWrite("ELF64 kernel loaded; entering long mode and calling kernel entry.\r\n");
    prepareLongMode(image.entryPoint, image.stackTop, &protocol);
    for (;;)
        __asm__ volatile ("cli; hlt");
}

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
/* -----------------------------------------------------------------------
 * UEFI file I/O wrapper for kernel loading
 * --------------------------------------------------------------------- */

static int uefiReadFile(void *context, uint64_t offset, void *buffer, size_t size) {
    UefiFileContext *ufc = (UefiFileContext *)context;
    EFI_STATUS status;
    UINTN readSize = (UINTN)size;

    if (ufc == NULL || ufc->file == NULL || buffer == NULL || size == 0U)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;

    status = ufc->file->SetPosition(ufc->file, offset);
    if (EFI_ERROR(status))
        return SYSTEM_CODE_ERROR_READ_FAULT;

    status = ufc->file->Read(ufc->file, &readSize, buffer);
    if (EFI_ERROR(status) || readSize != size)
        return SYSTEM_CODE_ERROR_READ_FAULT;

    return SYSTEM_CODE_SUCCESS;
}

static int uefiOpenFile(const char *path, UefiFileContext *ufc) {
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *file = NULL;
    EFI_FILE_INFO *info = NULL;
    UINTN infoSize = 0;
    CHAR16 uefiPath[128];
    UINTN i = 0U;

    if (gST == NULL || gST->BootServices == NULL || ufc == NULL || path == NULL)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;

    /* Convert ASCII path to CHAR16 for UEFI */
    while (path[i] != '\0' && i < (sizeof(uefiPath) / sizeof(CHAR16)) - 1U) {
        uefiPath[i] = (CHAR16)path[i];
        i++;
    }
    uefiPath[i] = (CHAR16)0;

    status = gST->BootServices->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid,
                                               NULL, (void **)&fs);
    if (EFI_ERROR(status))
        return SYSTEM_CODE_ERROR_NOT_FOUND;

    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status))
        return SYSTEM_CODE_ERROR_NOT_FOUND;

    status = root->Open(root, &file, uefiPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        root->Close(root);
        return SYSTEM_CODE_ERROR_FILE_NOT_FOUND;
    }
    root->Close(root);

    status = file->GetInfo(file, &gEfiFileInfoGuid, &infoSize, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        file->Close(file);
        return SYSTEM_CODE_ERROR_READ_FAULT;
    }

    info = NULL;
    status = gST->BootServices->AllocatePool(EfiLoaderData, infoSize, (void **)&info);
    if (EFI_ERROR(status)) {
        file->Close(file);
        return SYSTEM_CODE_ERROR_OUT_OF_MEMORY;
    }

    status = file->GetInfo(file, &gEfiFileInfoGuid, &infoSize, info);
    if (EFI_ERROR(status)) {
        gST->BootServices->FreePool(info);
        file->Close(file);
        return SYSTEM_CODE_ERROR_READ_FAULT;
    }

    ufc->file = file;
    ufc->size = info->FileSize;

    gST->BootServices->FreePool(info);
    return SYSTEM_CODE_SUCCESS;
}
#endif /* defined(EFI_BUILD) || defined(UEFI_BUILD) */

static void bootEntryEvent(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED)
        return;

    lv_obj_t *entry = lv_event_get_target(event);
    if (selectedBootEntry != NULL && selectedBootEntry != entry) {
        lv_obj_set_style_bg_color(selectedBootEntry, lv_color_hex(0xFFFFFFU), LV_PART_MAIN);
        lv_obj_set_style_border_color(selectedBootEntry, lv_color_hex(0xD8E0E9U), LV_PART_MAIN);
        lv_obj_t *old_label = lv_obj_get_child(selectedBootEntry, 0);
        if (old_label != NULL)
            lv_obj_set_style_text_color(old_label, lv_color_hex(0x344054U), LV_PART_MAIN);
    }

    selectedBootEntry = entry;
    lv_obj_set_style_bg_color(entry, lv_color_hex(0x3478D4U), LV_PART_MAIN);
    lv_obj_set_style_border_color(entry, lv_color_hex(0x245FAE), LV_PART_MAIN);
    lv_obj_t *label = lv_obj_get_child(entry, 0);
    if (label != NULL)
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFFU), LV_PART_MAIN);

    if ((uintptr_t)lv_event_get_user_data(event) == 2U)
        loadKernelBootEntry();
}

static void reboot_btn_cb(lv_event_t *event) {
    (void)event;
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
    outb(0x64U, 0xFEU);
#endif
    for (;;)
        __asm__ volatile ("hlt");
}

static void createBootMenu(char *menuItem[], int count) {
    uint32_t width = vesaGetWidth();
    uint32_t height = vesaGetHeight();
    lv_display_t *display = lv_display_create((int32_t)width, (int32_t)height);
    boot_display = display;
    debugInvalidationState("After display create:", display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, lvgl_draw_buffer, NULL, sizeof(lvgl_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flushDisplay);
    lv_display_set_dpi(display, 110);
    debugInvalidationState("After display setup:", display);

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, (int32_t)width, (int32_t)height);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF2F5F9U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_screen_load(screen);
    debugInvalidationState("After screen load:", display);

    uint32_t panel_width = width * 3U / 4U;
    uint32_t panel_height = height * 3U / 4U;
    int panel_x = (int)((width - panel_width) / 2U);
    int panel_y = (int)((height - panel_height) / 2U);
    int padding = (int)(panel_width / 20U);
    if (padding < 16)
        padding = 16;
    if ((uint32_t)(padding * 2) >= panel_width)
        padding = (int)(panel_width / 8U);

    lv_obj_t *panel = makePanel(screen, panel_x, panel_y, (int)panel_width,
                                (int)panel_height, 0xFFFFFFU, 0xD8E0E9U, 16);
    makeLabel(panel, "Boot manager", padding, padding,
              &lv_font_montserrat_20, 0x1D2939U);
    makeLabel(panel, "Select an operating system to start.", padding, padding + 32,
              &lv_font_montserrat_14, 0x667085U);

    int content_y = padding + 70;
    int content_height = (int)panel_height - content_y - padding - 1;
    if (content_height < 80)
        content_height = 80;
    lv_obj_t *content = makePanel(panel, padding, content_y,
                                  (int)panel_width - padding * 2, content_height,
                                  0xF8FAFCU, 0xE4E9F0U, 12);

    int content_width = (int)panel_width - padding * 2;
    lv_group_t *menu_group = NULL;
    const int row_height = 52;
    const int row_gap = 10;
    if (count > 0 && menuItem != NULL) {
        menu_group = lv_group_create();
        lv_group_set_wrap(menu_group, true);
        for (int i = 0; i < count; ++i) {
            const int row_y = 16 + i * (row_height + row_gap);
            lv_obj_t *entry = makePanel(content, 16, row_y, content_width - 32,
                                        row_height, 0xFFFFFFU, 0xD8E0E9U, 8);
            lv_obj_t *entry_label = makeLabel(entry,
                                              menuItem[i] != NULL ? menuItem[i] : "",
                                              16, 15, &lv_font_montserrat_14, 0x344054U);
            lv_obj_set_width(entry_label, content_width - 64);
            lv_obj_set_clickable(entry, true);
            lv_obj_add_event_cb(entry, bootEntryEvent, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            lv_obj_set_style_border_color(entry, lv_color_hex(0x3478D4U),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(entry, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_group_add_obj(menu_group, entry);
        }

        if(!secureMode) {
            const int row_y = 16 + count * (row_height + row_gap);
            lv_obj_t *entry = makePanel(content, 16, row_y, content_width - 32,
                                        row_height, 0xFFFFFFU, 0xD8E0E9U, 8);
            lv_obj_t *entry_label = makeLabel(entry,
                                              "Create New Entry",
                                              16, 15, &lv_font_montserrat_14, 0x344054U);
            lv_obj_set_width(entry_label, content_width - 64);
            lv_obj_set_clickable(entry, true);
            lv_obj_add_event_cb(entry, bootEntryEvent, LV_EVENT_CLICKED, NULL);
            lv_obj_set_style_border_color(entry, lv_color_hex(0x3478D4U),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(entry, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_group_add_obj(menu_group, entry);
        }

        lv_group_focus_obj(lv_group_get_obj_by_index(menu_group, 0));
    } else if (!secureMode) {
        menu_group = lv_group_create();
        lv_group_set_wrap(menu_group, true);
        const int row_y = 16 + 0 * (row_height + row_gap);
        lv_obj_t *entry = makePanel(content, 16, row_y, content_width - 32,
                                    row_height, 0xFFFFFFU, 0xD8E0E9U, 8);
        lv_obj_t *entry_label = makeLabel(entry,
                                          "Create New Entry",
                                          16, 15, &lv_font_montserrat_14, 0x344054U);
        lv_obj_set_width(entry_label, content_width - 64);
        lv_obj_set_clickable(entry, true);
        lv_obj_add_event_cb(entry, bootEntryEvent, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_border_color(entry, lv_color_hex(0x3478D4U),
                                       LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(entry, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_group_add_obj(menu_group, entry);

        lv_group_focus_obj(lv_group_get_obj_by_index(menu_group, 0));
    } else {
        int icon_y = content_height / 2 - 70;
        if (icon_y < 16)
            icon_y = 16;
        lv_obj_t *logo = makePanel(content, (content_width - 44) / 2, icon_y,
                                   44, 44, 0xE8F1FEU, 0xC7DAF5U, 12);
        (void)logo;
        lv_obj_t *drive = makePanel(content, (content_width - 24) / 2, icon_y + 12,
                                    24, 18, 0xFFFFFFU, 0x7AA5DFU, 4);
        (void)drive;
        lv_obj_t *drive_led = makePanel(content, (content_width - 24) / 2 + 15,
                                        icon_y + 23, 4, 4, 0x3478D4U, 0x3478D4U, 2);
        (void)drive_led;

        int text_y = icon_y + 68; /* Keep 24 px between the 44 px logo and title. */
        lv_obj_t *empty_title = makeLabel(content, "No boot entries configured", 16, text_y,
                                          &lv_font_montserrat_20, 0x344054U);
        lv_obj_set_width(empty_title, content_width - 32);
        lv_obj_set_style_text_align(empty_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_t *empty_hint = makeLabel(content, "This bootloader has no operating system entries yet.",
                                         16, text_y + 34, &lv_font_montserrat_14, 0x7A8699U);
        lv_obj_set_width(empty_hint, content_width - 32);
        lv_obj_set_style_text_align(empty_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    lv_indev_t *keyboard = lv_indev_create();
    lv_indev_set_type(keyboard, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keyboard, keyboardRead);
    lv_indev_set_display(keyboard, display);
    if (menu_group != NULL)
        lv_indev_set_group(keyboard, menu_group);

    mouse_set_bounds(width, height);
    lv_indev_t *mouse = lv_indev_create();
    boot_mouse = mouse;
    lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse, mouseRead);
    lv_indev_set_display(mouse, display);
    lv_obj_t *cursor = lv_obj_create(screen);
    lv_obj_remove_style_all(cursor);
    lv_obj_set_size(cursor, 12, 12);
    lv_obj_set_style_bg_color(cursor, lv_color_hex(0xFFFFFFU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(cursor, lv_color_hex(0x0A1422U), LV_PART_MAIN);
    lv_obj_set_style_border_width(cursor, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_clickable(cursor, false);
    lv_obj_set_hidden(cursor, true);
    cursor_visible = false;
    lv_indev_set_cursor(mouse, cursor);

    debugInvalidationState("After widget construction:", display);
    lv_result_t invalidation_result = lv_obj_invalidate(screen);
    PSerialWrite(lv_display_get_screen_active(display) == screen ?
                 "LVGL screen is active.\r\n" : "LVGL screen is not active.\r\n");
    PSerialWrite(lv_display_is_invalidation_enabled(display) ?
                 "LVGL invalidation is enabled.\r\n" : "LVGL invalidation is disabled.\r\n");
    PSerialWrite(invalidation_result == LV_RESULT_OK ?
                 "LVGL screen invalidation accepted.\r\n" : "LVGL screen invalidation rejected.\r\n");
    PSerialWrite("Before immediate LVGL refresh.\r\n");
    lv_refr_now(display);
    PSerialWrite("After immediate LVGL refresh.\r\n");
}

static void appendText(char *output, size_t capacity, size_t *used, const char *text) {
    while (*text != '\0' && *used + 1U < capacity)
        output[(*used)++] = *text++;
    output[*used] = '\0';
}

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
static void renderAncientBootMenu(char *menuItem[], int count) {
    int has_entries = count > 0 && menuItem != NULL;
    int entry_count = has_entries ? count + (secureMode ? 0 : 1) : (secureMode ? 0 : 1);
    int selected = 0;

    for (;;) {
        (void)call16((void *)setTextMode16, NULL, 0U);
        char output[512];
        size_t used = 0;
        output[0] = '\0';
        appendText(output, sizeof(output), &used, "Boot Manager\r\n");
        appendText(output, sizeof(output), &used,
                   "Use Up/Down to select and Enter to choose.\r\n\r\n");

        if (!has_entries) {
            appendText(output, sizeof(output), &used, "No boot entries configured.\r\n");
            if (!secureMode)
                appendText(output, sizeof(output), &used,
                           selected == 0 ? "> Create New Entry\r\n" : "  Create New Entry\r\n");
        } else {
            for (int i = 0; i < entry_count; ++i) {
                if (i == count) {
                    appendText(output, sizeof(output), &used,
                               selected == i ? "> Create New Entry\r\n" : "  Create New Entry\r\n");
                    continue;
                }
                appendText(output, sizeof(output), &used, i == selected ? "> " : "  ");
                appendText(output, sizeof(output), &used,
                           menuItem[i] != NULL ? menuItem[i] : "");
                appendText(output, sizeof(output), &used, "\r\n");
            }
        }
        printTL(output);

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        asm("sti");
#endif
        for (;;) {
            keyboard_poll();
            KeyboardEvent event;
            if (!keyboard_read_event(&event))
                continue;
            handleKeyboardHotkey(&event);
            if (!event.pressed)
                continue;

            if (entry_count > 0 && event.extended && event.scancode == 0x48U) {
                selected = selected == 0 ? entry_count - 1 : selected - 1;
                break;
            }
            if (entry_count > 0 && event.extended && event.scancode == 0x50U) {
                selected = (selected + 1) % entry_count;
                break;
            }
            if (!event.extended && event.scancode == 0x1CU) {
                if (has_entries && selected < count) {
                    printTL("\r\nSelected: ");
                    printTL(menuItem[selected] != NULL ? menuItem[selected] : "");
                    printTL("\r\n");
                    if (selected == 2)
                        loadKernelBootEntry();
                } else if (!secureMode)
                    printTL("\r\nSelected: Create New Entry\r\n");
                else
                    printTL("\r\nNo boot entries configured.\r\n");
                for (;;) {
                    keyboard_poll();
                    KeyboardEvent next_event;
                    if (keyboard_read_event(&next_event)) {
                        handleKeyboardHotkey(&next_event);
                        if (!next_event.pressed)
                            continue;
                        break;
                    }
                }
                break;
            }
        }
    }
}
#else
static void renderAncientBootMenu(char *menuItem[], int count) {
    (void)menuItem;
    (void)count;
    PSerialWrite("UEFI console menu is handled by EFI services.\r\n");
}
#endif

void PANIC(char *message) {
    /* The on-screen report is invisible on a serial-only console, so always
     * mirror the reason to COM1 first. */
    PSerialWrite("PANIC: ");
    PSerialWrite(message != NULL ? message : "(no message)");
    PSerialWrite("\r\n");
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    /* No VBE framebuffer or LVGL under UEFI; just halt. */
    for (;;)
        __asm__ volatile ("cli; hlt");
#endif
    if (ancientTheme == true) {
#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
        (void)call16((void *)setTextMode16, NULL, 0U);
#endif
        printTL(message);
        die();
    } else {
        if (boot_display == NULL) {
            mouse_init();
            lv_init();
            uint32_t width = vesaGetWidth();
            uint32_t height = vesaGetHeight();
            boot_display = lv_display_create((int32_t)width, (int32_t)height);
            lv_display_set_color_format(boot_display, LV_COLOR_FORMAT_XRGB8888);
            lv_display_set_buffers(boot_display, lvgl_draw_buffer, NULL,
                                  sizeof(lvgl_draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
            lv_display_set_flush_cb(boot_display, flushDisplay);
            mouse_set_bounds(width, height);
        }
        lv_obj_t * scr = lv_obj_create(NULL);
        lv_screen_load(scr);
        lv_obj_set_style_bg_color(scr, lv_color_hex(0xE6E6E6), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
        
        lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(scr, 25, LV_PART_MAIN);

        lv_obj_t * icon = lv_label_create(scr);
        lv_label_set_text(icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x333333), LV_PART_MAIN);

        lv_obj_t * title = lv_label_create(scr);
        lv_label_set_text(title, "Sorry!");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(0x2E3436), LV_PART_MAIN);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        lv_obj_t * subtext = lv_label_create(scr);
        lv_label_set_text_fmt(subtext, "%s\n\nA problem has occurred\n"
                                         "Please contact a system administrator.",
                             message);
        lv_obj_set_style_text_font(subtext, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(subtext, lv_color_hex(0x555753), LV_PART_MAIN);
        lv_obj_set_style_text_align(subtext, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        lv_obj_t * btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 140, 42);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x007BFF), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0069D9), LV_STATE_HOVERED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0056B3), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(btn, reboot_btn_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, "Reboot");
        lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(btn_label);
        lv_group_t *error_group = lv_group_create();
        lv_group_add_obj(error_group, btn);
        lv_group_focus_obj(btn);
        if (boot_keyboard == NULL) {
            boot_keyboard = lv_indev_create();
            lv_indev_set_type(boot_keyboard, LV_INDEV_TYPE_KEYPAD);
            lv_indev_set_read_cb(boot_keyboard, keyboardRead);
            lv_indev_set_display(boot_keyboard, boot_display);
        }
        lv_indev_set_group(boot_keyboard, error_group);
        if (boot_mouse == NULL) {
            boot_mouse = lv_indev_create();
            lv_indev_set_type(boot_mouse, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(boot_mouse, mouseRead);
            lv_indev_set_display(boot_mouse, boot_display);
        }
        lv_obj_t *cursor = lv_obj_create(scr);
        lv_obj_remove_style_all(cursor);
        lv_obj_set_size(cursor, 12, 12);
        lv_obj_set_style_bg_color(cursor, lv_color_hex(0xFFFFFFU), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(cursor, lv_color_hex(0x0A1422U), LV_PART_MAIN);
        lv_obj_set_style_border_width(cursor, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_clickable(cursor, false);
        lv_indev_set_cursor(boot_mouse, cursor);
        lv_refr_now(boot_display);
        uint16_t previous_pit_counter = readPitCounter();
        for (;;) {
            mouse_poll();
            uint16_t pit_counter = readPitCounter();
            if (pit_counter > previous_pit_counter)
                lv_tick_inc(55U);
            previous_pit_counter = pit_counter;
            lv_timer_handler();
        }

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        asm("sti");
#endif
    }
}

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
EFI_SYSTEM_TABLE *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
EFI_HANDLE gImageHandle = NULL;
#endif

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable != NULL ? SystemTable->BootServices : NULL;
#else
void lmain(void) {
    typedef struct __attribute__((packed)) {
        uint8_t  e_ident[16];
        uint16_t e_type;
        uint16_t e_machine;
        uint32_t e_version;
        uint64_t e_entry;
        uint64_t e_phoff;
        uint64_t e_shoff;
        uint32_t e_flags;
        uint16_t e_ehsize;
        uint16_t e_phentsize;
        uint16_t e_phnum;
        uint16_t e_shentsize;
        uint16_t e_shnum;
        uint16_t e_shstrndx;
    } Elf64_Ehdr;
    typedef struct __attribute__((packed)) {
        uint32_t p_type;
        uint32_t p_flags;
        uint64_t p_offset;
        uint64_t p_vaddr;
        uint64_t p_paddr;
        uint64_t p_filesz;
        uint64_t p_memsz;
        uint64_t p_align;
    } Elf64_Phdr;
    typedef struct __attribute__((packed)) {
        uint64_t handleSyscall;
        uint64_t vbeInfo;
        uint64_t vbeModeInfo;
        uint32_t currentMode;
        uint32_t firmware;
        uint64_t imageHandle;
        uint64_t systemTable;
    } KernelBootProtocol;

    const uint8_t *kernel = (const uint8_t *)(uintptr_t)0x90000U;
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)kernel;
    if (ehdr->e_ident[0] != 0x7FU || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F' ||
        ehdr->e_ident[4] != 2U || ehdr->e_phentsize < sizeof(Elf64_Phdr) ||
        ehdr->e_phoff > 12288U ||
        (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > 12288U - ehdr->e_phoff) {
        PSerialWrite("Invalid /oskrnl ELF image.\r\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    }

    uint32_t loaded_segments = 0U;
    for (uint16_t i = 0; i < ehdr->e_phnum; ++i) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(kernel + ehdr->e_phoff +
                                             (uint64_t)i * ehdr->e_phentsize);
        if (phdr->p_type != 1U || phdr->p_memsz == 0U)
            continue;
        if (phdr->p_filesz > phdr->p_memsz || phdr->p_offset > 12288U ||
            phdr->p_filesz > 12288U - phdr->p_offset ||
            phdr->p_vaddr < 0x100000U || phdr->p_vaddr >= 0x100000000ULL ||
            phdr->p_memsz > 0x100000000ULL - phdr->p_vaddr) {
            PSerialWrite("Invalid /oskrnl load segment.\r\n");
            for (;;)
                __asm__ volatile ("cli; hlt");
        }
        uint8_t *destination = (uint8_t *)(uintptr_t)phdr->p_vaddr;
        for (uint64_t j = 0; j < phdr->p_filesz; ++j)
            destination[j] = kernel[phdr->p_offset + j];
        for (uint64_t j = phdr->p_filesz; j < phdr->p_memsz; ++j)
            destination[j] = 0U;
        ++loaded_segments;
    }

    if (loaded_segments == 0U) {
        PSerialWrite("No loadable segments in /oskrnl.\r\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    }

    KernelBootProtocol protocol = {
        .handleSyscall = 0U,
        .vbeInfo = (uint64_t)(uintptr_t)&g_vbe_info,
        .vbeModeInfo = (uint64_t)(uintptr_t)&g_vbe_mode_info,
        .currentMode = (uint32_t)curVBEMode,
        .firmware = BIOS,
        .imageHandle = 0,
        .systemTable = 0,
    };

    uint64_t entry = ehdr->e_entry;
    uintptr_t stack_top = 0x1200000U;

    __asm__ volatile (
        "push %5\n"
        "push %4\n"
        "push %3\n"
        "push %2\n"
        "push %1\n"
        "push %0\n"
        "call prepareLongMode\n"
        "add $24, %%esp\n"
        :
        : "r"((uint32_t)entry), "r"((uint32_t)(entry >> 32)),
          "r"((uint32_t)stack_top), "i"(0),
          "r"((uint32_t)(uintptr_t)&protocol), "i"(0)
        : "memory"
    );

    for (;;)
        __asm__ volatile ("cli; hlt");
#endif
}