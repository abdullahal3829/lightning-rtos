/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include "vesa.h"
#include "lvgl.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "fs/toritofs.h"
#include "kloader/kloader.h"
#include "mm.h"
#include <definations.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern void die();
extern uint8_t bootdrive;
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

extern int call16(void *address, void *args, size_t size);
extern uint8_t printTL16[];
extern uint8_t setTextMode16[];
void PSerialWrite(const char *str);
static void loadKernelBootEntry(void);

static void printTL(const char *text) {
    size_t length = 0;
    while (text[length] != '\0')
        ++length;
    (void)call16((void *)printTL16, (void *)text, length + 1U);
}

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
    for (uint32_t i = 0; i < SERIAL_PORT_COUNT; ++i) {
        uint16_t port = serialPortAt(i);
        if (!serialPortPresent(port))
            continue;

        /* Configure each BIOS-reported 16550 UART for 115200 8N1. */
        outb(port + UART_INTERRUPT_ENABLE, 0x00);
        outb(port + UART_LINE_CONTROL, 0x80); /* Enable divisor latch. */
        outb(port + UART_DATA, 0x01);         /* Divisor low byte. */
        outb(port + UART_INTERRUPT_ENABLE, 0x00);
        outb(port + UART_LINE_CONTROL, 0x03); /* 8 data bits, no parity, 1 stop. */
        outb(port + UART_FIFO_CONTROL, 0xC7);
        outb(port + 4, 0x0B);                /* DTR, RTS, and OUT2. */
    }
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
extern VbeInfoBlock g_vbe_info;
extern struct vbe_mode_info_structure g_vbe_mode_info;
extern int vbeIsInited;
extern uint16_t curVBEMode;

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

static uint16_t readPitCounter(void) {
    outb(0x43U, 0x00U); /* Latch channel 0's current countdown value. */
    uint8_t low = inb(0x40U);
    uint8_t high = inb(0x40U);
    return (uint16_t)(((uint16_t)high << 8) | low);
}

static uint32_t keyboardEventToLvglKey(const KeyboardEvent *event) {
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

static void flushDisplay(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    static bool flush_logged;
    uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
    if (!flush_logged) {
        flush_logged = true;
        PSerialWrite("Framebuffer flush callback reached.\r\n");
    }
    vesaFlush((uint32_t)area->x1, (uint32_t)area->y1, width, height, (const uint32_t *)pixels);
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

static void loadKernelBootEntry(void) {
    ToritoFile source;
    KernelFile kernel = {
        .read = toritoReadFile,
        .context = &source,
        .size = 0U
    };
    KernelImage image;
    PSerialWrite("Loading boot entry /oskrnl.\r\n");
    PSerialWrite("Boot drive: ");
    PSerialWriteHex((uint16_t)bootdrive);
    PSerialWrite("\r\n");
    int result = toritoOpenFile(bootdrive, "/oskrnl", &source);
    if (result == SYSTEM_CODE_ERROR_FILE_NOT_FOUND) {
        PSerialWrite("Kernel file /oskrnl was not found on the boot media.\r\n");
        PANIC("A Specified File Not Found");
        return;
    }
    if (result != SYSTEM_CODE_SUCCESS) {
        PSerialWrite("Opening /oskrnl failed with code ");
        PSerialWriteHex((uint16_t)result);
        PSerialWrite("\r\n");
        PANIC("Invalid Kernel");
        return;
    }
    PSerialWrite("Kernel file found; reading ELF image.\r\n");
    kernel.size = source.size;
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

    /* This is the packed 64-bit protocol layout consumed by an ELF64 kernel.
     * The loader itself is compiled as 32-bit, so its native EBootProtocol
     * pointer fields are only four bytes wide. */
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
        .vbeInfo = (uint64_t)(uintptr_t)&g_vbe_info,
        .vbeModeInfo = (uint64_t)(uintptr_t)&g_vbe_mode_info,
        .currentMode = (uint32_t)curVBEMode,
        .firmware = 0U, /* BIOS */
        .imageHandle = 0U,
        .systemTable = 0U
    };

    PSerialWrite("ELF64 kernel loaded; entering long mode and calling kernel entry.\r\n");
    prepareLongMode(image.entryPoint, image.stackTop, &protocol);
}

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
    outb(0x64U, 0xFEU);
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

void PANIC(char *message) {
    /* The on-screen report is invisible on a serial-only console, so always
     * mirror the reason to COM1 first. */
    PSerialWrite("PANIC: ");
    PSerialWrite(message != NULL ? message : "(no message)");
    PSerialWrite("\r\n");
    if (ancientTheme == true) {
        (void)call16((void *)setTextMode16, NULL, 0U);
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
        
        // Use Flex layout to arrange elements vertically down the center
        lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(scr, 25, LV_PART_MAIN); // Space between components

        /* 2. The Frowny Computer Icon (Using LVGL built-in symbol as placeholder) */
        lv_obj_t * icon = lv_label_create(scr);
        lv_label_set_text(icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x333333), LV_PART_MAIN);

        /* 3. Main Header Text */
        lv_obj_t * title = lv_label_create(scr);
        lv_label_set_text(title, "Sorry!");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(0x2E3436), LV_PART_MAIN);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        /* 4. Secondary Subtext */
        lv_obj_t * subtext = lv_label_create(scr);
        lv_label_set_text_fmt(subtext, "%s\n\nA problem has occurred\n"
                                         "Please contact a system administrator.",
                             message);
        lv_obj_set_style_text_font(subtext, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(subtext, lv_color_hex(0x555753), LV_PART_MAIN);
        lv_obj_set_style_text_align(subtext, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        /* 5. Action Button */
        lv_obj_t * btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 140, 42);

        /* Themes are disabled in boot/lv_conf.h, so make the background
         * opaque explicitly or the button's color remains transparent. */
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x007BFF), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0069D9), LV_STATE_HOVERED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0056B3), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 6, LV_PART_MAIN); // Slightly rounded corners
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN); // Clean flat aesthetic
        
        // Attach the event callback
        lv_obj_add_event_cb(btn, reboot_btn_cb, LV_EVENT_CLICKED, NULL);

        /* 6. Button Text Label */
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

void lmain(void) {
    clearBssAndPreserveVideoState();
    serialInit();
    PSerialWrite("ChainLoaded\r\n");
    PSerialWrite(keyboard_init() ? "Keyboard PS/2 OK\r\n" : "Keyboard PS/2 unavailable\r\n");
    char *sampleData[] = {
        "X OS",
        "Y Option",
        "OS Kernel (/oskrnl)"
    };
    secureMode = false;
    if (ancientTheme)
        renderAncientBootMenu(sampleData, 3);

    PSerialWrite(mouse_init() ? "Mouse PS/2 OK\r\n" : "Mouse PS/2 unavailable\r\n");
    if (headlessMode) {
        /* Nothing can be clicked or typed on a machine without a display, so
         * the graphical menu would only delay the kernel from starting. */
        PSerialWrite("Headless boot: skipping the graphical boot menu.\r\n");
        loadKernelBootEntry();
        PSerialWrite("Kernel entry returned without jumping; halting.\r\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    }
    if (fallocAt(0x00200000ULL, 128U * 1024U) == NULL) {
        PSerialWrite("LVGL memory pool reservation failed.\r\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    PSerialWrite("LVGL 128 KiB pool reserved at 0x00200000.\r\n");
    lv_init();
    createBootMenu(sampleData, 3);
    uint16_t previous_pit_counter = readPitCounter();
    uint32_t menu_started = lv_tick_get();
    for (;;) {
        keyboard_poll();
        mouse_poll();
        uint16_t pit_counter = readPitCounter();
        if (pit_counter > previous_pit_counter)
            lv_tick_inc(55U); /* PIT channel 0 wraps about every 54.9 ms. */
        previous_pit_counter = pit_counter;
        /* A serial-only console cannot operate the menu, so fall back to the
         * kernel entry when nobody makes a selection in time. */
        if ((uint32_t)(lv_tick_get() - menu_started) >= 10000U) {
            PSerialWrite("Boot menu timed out; starting the kernel entry.\r\n");
            loadKernelBootEntry();
            menu_started = lv_tick_get();
        }
        lv_timer_handler();
    }
}