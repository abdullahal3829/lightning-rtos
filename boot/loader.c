#include "vesa.h"
#include "lvgl.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
bool secureMode = false;
bool ancientTheme = false;

extern void *call16(void *address, void *args, size_t size);
extern uint8_t printTL16[];
extern uint8_t setTextMode16[];

static void printTL(const char *text) {
    size_t length = 0;
    while (text[length] != '\0')
        ++length;
    (void)call16((void *)printTL16, (void *)text, length + 1U);
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
extern int vbeIsInited;
extern uint16_t curVBEMode;

static uint32_t lvgl_draw_buffer[800U * 24U];
static bool cursor_visible;
static bool mouse_position_initialized;
static int32_t last_mouse_x;
static int32_t last_mouse_y;
static uint32_t last_mouse_activity;

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
    uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
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
}

static void createBootMenu(char *menuItem[], int count) {
    uint32_t width = vesaGetWidth();
    uint32_t height = vesaGetHeight();
    lv_display_t *display = lv_display_create((int32_t)width, (int32_t)height);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, lvgl_draw_buffer, NULL, sizeof(lvgl_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flushDisplay);
    lv_display_set_dpi(display, 110);

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, (int32_t)width, (int32_t)height);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF2F5F9U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_screen_load(screen);

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
            lv_obj_set_flag(entry, LV_OBJ_FLAG_CLICKABLE, true);
            lv_obj_add_event_cb(entry, bootEntryEvent, LV_EVENT_CLICKED, NULL);
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
            lv_obj_set_flag(entry, LV_OBJ_FLAG_CLICKABLE, true);
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
        lv_obj_set_flag(entry, LV_OBJ_FLAG_CLICKABLE, true);
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
    lv_obj_set_flag(cursor, LV_OBJ_FLAG_CLICKABLE, false);
    lv_obj_set_hidden(cursor, true);
    cursor_visible = false;
    lv_indev_set_cursor(mouse, cursor);

    lv_refr_now(display);
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

        for (;;) {
            keyboard_poll();
            KeyboardEvent event;
            if (!keyboard_read_event(&event) || !event.pressed)
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
                } else if (!secureMode) {
                    printTL("\r\nSelected: Create New Entry\r\n");
                } else {
                    printTL("\r\nNo boot entries configured.\r\n");
                }
                for (;;) {
                    keyboard_poll();
                    KeyboardEvent next_event;
                    if (keyboard_read_event(&next_event) && next_event.pressed)
                        break;
                }
                break;
            }
        }
    }
}

void lmain(void) {
    clearBssAndPreserveVideoState();
    serialInit();
    PSerialWrite("ChainLoaded\r\n");
    PSerialWrite(keyboard_init() ? "Keyboard PS/2 OK\r\n" : "Keyboard PS/2 unavailable\r\n");
    char *sampleData[] = {
        "X OS",
        "Y Option"
    };
    secureMode = false;
    if (ancientTheme)
        renderAncientBootMenu(sampleData, 2);

    PSerialWrite(mouse_init() ? "Mouse PS/2 OK\r\n" : "Mouse PS/2 unavailable\r\n");
    lv_init();
    createBootMenu(sampleData, 2);
    uint16_t previous_pit_counter = readPitCounter();
    for (;;) {
        keyboard_poll();
        mouse_poll();
        uint16_t pit_counter = readPitCounter();
        if (pit_counter > previous_pit_counter)
            lv_tick_inc(55U); /* PIT channel 0 wraps about every 54.9 ms. */
        previous_pit_counter = pit_counter;
        lv_timer_handler();
    }
}
