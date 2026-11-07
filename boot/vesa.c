/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdint.h>
#include "mm.h"
#include "vesa.h"

#define VGA_FRAMEBUFFER_BASE 0x000A0000UL
#define VBE_DISPI_INDEX_PORT 0x01CEU
#define VBE_DISPI_DATA_PORT  0x01CFU
#define VBE_DISPI_INDEX_ID   0x0000U
#define VBE_DISPI_ID_MIN    0xB0C0U
#define VBE_DISPI_ENABLE     0x0004U
#define VBE_DISPI_XRES       0x0001U
#define VBE_DISPI_YRES       0x0002U
#define VBE_DISPI_BPP        0x0003U
#define VBE_DISPI_VIRT_WIDTH 0x0006U
#define VBE_DISPI_ENABLED    0x0001U
#define VBE_DISPI_LFB_ENABLE 0x0040U
#define PCI_CONFIG_ADDRESS   0x0CF8U
#define PCI_CONFIG_DATA      0x0CFCU

extern struct vbe_mode_info_structure g_vbe_mode_info;
extern void PSerialWrite(const char *text);

static struct {
    uint32_t base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} g_framebuffer = {0};

static void debugHex32(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char text[] = "0x00000000";
    for (uint32_t i = 0; i < 8U; ++i) {
        text[9U - i] = digits[value & 0x0FU];
        value >>= 4U;
    }
    PSerialWrite(text);
}

static void debugValue(const char *label, uint32_t value) {
    PSerialWrite(label);
    debugHex32(value);
    PSerialWrite("\r\n");
}

static uint32_t debug_flush_count;
static uint32_t debug_rejected_flush_count;

static void reserveFramebufferMemory(uint32_t base, uint32_t width, uint32_t height, uint32_t pitch) {
    uint64_t frame_size;
    void *reservation;

    if (base == 0U || width == 0U || height == 0U || pitch == 0U)
        return;

    frame_size = (uint64_t)height * (uint64_t)pitch;
    if (frame_size == 0U)
        return;

    /* The allocator must never reuse the active video RAM range for heap storage.
     * Mark the physical framebuffer as reserved in the BIOS E820 map before any
     * later falloc() calls can pick it up as a usable block. */
    reservation = fallocAt((uint64_t)base, (size_t)frame_size);
    PSerialWrite(reservation != 0 ? "VRAM allocator reservation: success\r\n" :
                                    "VRAM allocator reservation: outside usable map\r\n");
}

static uint32_t pciRead32(uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)device << 11U) |
                       ((uint32_t)function << 8U) |
                       ((uint32_t)offset & 0xFCU);
    uint32_t value;
    __asm__ __volatile__("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
    __asm__ __volatile__("inl %1, %0" : "=a"(value) : "Nd"(PCI_CONFIG_DATA));
    return value;
}

static void pciWrite32(uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

static uint32_t findVgaFramebufferBase(void) {
    for (uint8_t device = 0; device < 32U; ++device) {
        for (uint8_t function = 0; function < 8U; ++function) {
            uint32_t id = pciRead32(device, function, 0x00U);
            if ((id & 0xFFFFU) == 0xFFFFU)
                continue;

            uint32_t class_code = pciRead32(device, function, 0x08U);
            if ((class_code >> 24U) != 0x03U)
                continue;

            uint32_t command_status = pciRead32(device, function, 0x04U);
            pciWrite32(device, function, 0x04U, command_status | 0x00000006U);
            uint32_t bar0 = pciRead32(device, function, 0x10U);
            uint32_t framebuffer = bar0 & 0xFFFFFFF0U;
            if ((bar0 & 1U) == 0U && framebuffer != 0U)
                return framebuffer;
        }
    }
    return 0U;
}

static uint16_t vbeDispiRead(uint16_t index) {
    uint16_t value;
    __asm__ __volatile__("outw %w0, %w1" : : "a"(index), "Nd"(VBE_DISPI_INDEX_PORT));
    __asm__ __volatile__("inw %w1, %w0" : "=a"(value) : "Nd"(VBE_DISPI_DATA_PORT));
    return value;
}

static void vbeDispiWrite(uint16_t index, uint16_t value) {
    __asm__ __volatile__("outw %w0, %w1" : : "a"(index), "Nd"(VBE_DISPI_INDEX_PORT));
    __asm__ __volatile__("outw %w0, %w1" : : "a"(value), "Nd"(VBE_DISPI_DATA_PORT));
}

static void pciWrite32(uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000U |
                       ((uint32_t)device << 11U) |
                       ((uint32_t)function << 8U) |
                       ((uint32_t)offset & 0xFCU);
    __asm__ __volatile__("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(PCI_CONFIG_DATA));
}

static int configureQemuLinearFramebuffer(void) {
    uint16_t id = vbeDispiRead(VBE_DISPI_INDEX_ID);
    if (id < VBE_DISPI_ID_MIN || id > VBE_DISPI_ID_MIN + 5U)
        return 0;

    uint32_t framebuffer_base = findVgaFramebufferBase();
    if (framebuffer_base == 0U)
        return 0;

    /* QEMU's Bochs VBE device exposes the linear framebuffer in PCI BAR0. */
    vbeDispiWrite(VBE_DISPI_ENABLE, 0U);
    vbeDispiWrite(VBE_DISPI_XRES, 800U);
    vbeDispiWrite(VBE_DISPI_YRES, 600U);
    vbeDispiWrite(VBE_DISPI_BPP, 32U);
    vbeDispiWrite(VBE_DISPI_VIRT_WIDTH, 800U);
    /* Bit 0 enables the VBE device; bit 6 selects its linear framebuffer. */
    vbeDispiWrite(VBE_DISPI_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLE);
    if (vbeDispiRead(VBE_DISPI_XRES) != 800U ||
        vbeDispiRead(VBE_DISPI_YRES) != 600U ||
        vbeDispiRead(VBE_DISPI_BPP) != 32U ||
        (vbeDispiRead(VBE_DISPI_ENABLE) & (VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLE)) !=
            (VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLE))
        return 0;

    g_framebuffer.base = framebuffer_base;
    g_framebuffer.width = 800U;
    g_framebuffer.height = 600U;
    g_framebuffer.pitch = 800U * 4U;
    g_framebuffer.bpp = 32U;
    PSerialWrite("QEMU linear framebuffer configured.\r\n");
    debugValue("VRAM base: ", framebuffer_base);
    debugValue("VRAM width: ", g_framebuffer.width);
    debugValue("VRAM height: ", g_framebuffer.height);
    debugValue("VRAM pitch: ", g_framebuffer.pitch);
    debugValue("VRAM BPP: ", g_framebuffer.bpp);
    reserveFramebufferMemory(framebuffer_base, 800U, 600U, 800U * 4U);
    return 1;
}

VbeInfoBlock g_vbe_info __attribute__((section(".lowdata"))) = {0};
int vbeIsInited __attribute__((section(".lowdata"))) = 0;
uint16_t curVBEMode __attribute__((section(".lowdata"))) = 0U;

static void refreshFramebufferState(void) {
    if (g_framebuffer.base != 0U)
        return;

    uint32_t width = g_vbe_mode_info.width;
    uint32_t height = g_vbe_mode_info.height;
    uint32_t pitch = g_vbe_mode_info.pitch;
    uint8_t bpp = g_vbe_mode_info.bpp;
    uint32_t base = g_vbe_mode_info.framebuffer;

    if ((g_vbe_mode_info.attributes & 0x0080U) == 0U || base == 0U ||
        width == 0U || height == 0U || pitch == 0U || bpp == 0U) {
        if (configureQemuLinearFramebuffer())
            return;

        base = (uint32_t)VGA_FRAMEBUFFER_BASE;
        width = 320U;
        height = 200U;
        bpp = 8U;
        pitch = 320U;
    }

    if (pitch == 0U)
        pitch = width * ((uint32_t)bpp + 7U) / 8U;

    g_framebuffer.base = base;
    g_framebuffer.width = width;
    g_framebuffer.height = height;
    g_framebuffer.pitch = pitch;
    g_framebuffer.bpp = bpp;

    PSerialWrite("VBE framebuffer state selected.\r\n");
    debugValue("VRAM base: ", base);
    debugValue("VRAM width: ", width);
    debugValue("VRAM height: ", height);
    debugValue("VRAM pitch: ", pitch);
    debugValue("VRAM BPP: ", bpp);
    reserveFramebufferMemory(base, width, height, pitch);
}

uint32_t vesaGetWidth(void) {
    refreshFramebufferState();
    return g_framebuffer.width;
}

uint32_t vesaGetHeight(void) {
    refreshFramebufferState();
    return g_framebuffer.height;
}

void vesaFlush(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const uint32_t *pixels) {
    uint32_t flush_number = ++debug_flush_count;
    refreshFramebufferState();
    if (pixels == 0 || x >= g_framebuffer.width || y >= g_framebuffer.height) {
        if (debug_rejected_flush_count++ < 16U) {
            PSerialWrite("Rejected flush: null source or origin outside screen.\r\n");
            debugValue("flush x: ", x);
            debugValue("flush y: ", y);
            debugValue("screen width: ", g_framebuffer.width);
            debugValue("screen height: ", g_framebuffer.height);
        }
        return;
    }

    if (width > g_framebuffer.width - x)
        width = g_framebuffer.width - x;
    if (height > g_framebuffer.height - y)
        height = g_framebuffer.height - y;

    if (g_framebuffer.bpp != 32U || g_framebuffer.pitch < g_framebuffer.width * 4U ||
        (g_framebuffer.pitch & 3U) != 0U) {
        if (debug_rejected_flush_count++ < 16U) {
            PSerialWrite("Rejected flush: unsupported pixel format or pitch.\r\n");
            debugValue("flush BPP: ", g_framebuffer.bpp);
            debugValue("flush pitch: ", g_framebuffer.pitch);
            debugValue("minimum pitch: ", g_framebuffer.width * 4U);
        }
        return;
    }

    if (flush_number <= 32U) {
        PSerialWrite("Framebuffer flush #");
        debugHex32(flush_number);
        PSerialWrite(" area ");
        debugHex32(x);
        PSerialWrite(",");
        debugHex32(y);
        PSerialWrite(" size ");
        debugHex32(width);
        PSerialWrite("x");
        debugHex32(height);
        PSerialWrite(" source[0]=");
        debugHex32(pixels[0]);
        PSerialWrite("\r\n");
    } else if (flush_number == 33U) {
        PSerialWrite("Framebuffer flush trace capped at 32 calls.\r\n");
    }

    volatile uint32_t *framebuffer = (volatile uint32_t *)(uintptr_t)g_framebuffer.base;
    uint32_t pitch_pixels = g_framebuffer.pitch / sizeof(uint32_t);
    uint32_t first_pixel_before = framebuffer[y * pitch_pixels + x];
    for (uint32_t row = 0; row < height; ++row) {
        uint32_t destination = (y + row) * pitch_pixels + x;
        uint32_t source = row * width;
        for (uint32_t column = 0; column < width; ++column)
            framebuffer[destination + column] = pixels[source + column];
    }
    if (flush_number <= 32U) {
        PSerialWrite("VRAM first pixel before=");
        debugHex32(first_pixel_before);
        PSerialWrite(" after=");
        debugHex32(framebuffer[y * pitch_pixels + x]);
        PSerialWrite("\r\n");
    }
}

struct VbeInfoBlock getExtensionInfo() {
    if(vbeIsInited == 1)
        return g_vbe_info;
    return (struct VbeInfoBlock){0};
}

void drawFramebuffer(int x, int y, uint64_t color) {
    uint32_t bytes_per_pixel;
    uint64_t offset;
    uint64_t frame_size;
    uint8_t *framebuffer;
    uint32_t pixel_color;

    if (x < 0 || y < 0)
        return;

    refreshFramebufferState();

    if (g_framebuffer.base == 0U || g_framebuffer.width == 0U || g_framebuffer.height == 0U || g_framebuffer.pitch == 0U)
        return;

    if ((uint32_t)x >= g_framebuffer.width || (uint32_t)y >= g_framebuffer.height)
        return;

    pixel_color = (uint32_t)(color & 0xFFFFFFFFU);
    bytes_per_pixel = (g_framebuffer.bpp > 8U) ? (uint32_t)(g_framebuffer.bpp / 8U) : 1U;
    if (bytes_per_pixel == 0U || bytes_per_pixel > 4U)
        return;

    offset = ((uint64_t)(uint32_t)y * (uint64_t)g_framebuffer.pitch) + ((uint64_t)(uint32_t)x * (uint64_t)bytes_per_pixel);
    frame_size = (uint64_t)g_framebuffer.pitch * (uint64_t)g_framebuffer.height;
    if (offset + (uint64_t)bytes_per_pixel > frame_size)
        return;

    framebuffer = (uint8_t *)(uintptr_t)g_framebuffer.base;

    switch (g_framebuffer.bpp) {
        case 8U:
            framebuffer[offset] = (uint8_t)(pixel_color & 0xFFU);
            break;
        case 16U:
            *(uint16_t *)(framebuffer + offset) = (uint16_t)(pixel_color & 0xFFFFU);
            break;
        case 24U:
            framebuffer[offset + 0U] = (uint8_t)(pixel_color & 0xFFU);
            framebuffer[offset + 1U] = (uint8_t)((pixel_color >> 8U) & 0xFFU);
            framebuffer[offset + 2U] = (uint8_t)((pixel_color >> 16U) & 0xFFU);
            break;
        case 32U:
            *(uint32_t *)(framebuffer + offset) = pixel_color;
            break;
        default:
            return;
    }
}