#include <stdint.h>
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
#define VBE_DISPI_LFB_ENABLE 0x0041U
#define PCI_CONFIG_ADDRESS   0x0CF8U
#define PCI_CONFIG_DATA      0x0CFCU

extern struct vbe_mode_info_structure g_vbe_mode_info;

static struct {
    uint32_t base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t bpp;
} g_framebuffer = {0};

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

static uint32_t findVgaFramebufferBase(void) {
    for (uint8_t device = 0; device < 32U; ++device) {
        for (uint8_t function = 0; function < 8U; ++function) {
            uint32_t id = pciRead32(device, function, 0x00U);
            if ((id & 0xFFFFU) == 0xFFFFU)
                continue;

            uint32_t class_code = pciRead32(device, function, 0x08U);
            if ((class_code >> 24U) != 0x03U)
                continue;

            uint32_t bar0 = pciRead32(device, function, 0x10U);
            if ((bar0 & 1U) == 0U)
                return bar0 & 0xFFFFFFF0U;
        }
    }
    return 0U;
}

static uint16_t vbeDispiRead(uint16_t index) {
    uint16_t value;
    __asm__ __volatile__("outw %0, %1" : : "a"(index), "Nd"(VBE_DISPI_INDEX_PORT));
    __asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"(VBE_DISPI_DATA_PORT));
    return value;
}

static void vbeDispiWrite(uint16_t index, uint16_t value) {
    __asm__ __volatile__("outw %0, %1" : : "a"(index), "Nd"(VBE_DISPI_INDEX_PORT));
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(VBE_DISPI_DATA_PORT));
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
    vbeDispiWrite(VBE_DISPI_XRES, 1280U);
    vbeDispiWrite(VBE_DISPI_YRES, 1024U);
    vbeDispiWrite(VBE_DISPI_BPP, 32U);
    vbeDispiWrite(VBE_DISPI_VIRT_WIDTH, 1280U);
    vbeDispiWrite(VBE_DISPI_ENABLE, VBE_DISPI_LFB_ENABLE);

    g_framebuffer.base = framebuffer_base;
    g_framebuffer.width = 1280U;
    g_framebuffer.height = 1024U;
    g_framebuffer.pitch = 1280U * 4U;
    g_framebuffer.bpp = 32U;
    return 1;
}

VbeInfoBlock g_vbe_info = {0};
int vbeIsInited = 0;
uint16_t curVBEMode = 0U;

static void refreshFramebufferState(void) {
    if (g_framebuffer.base != 0U)
        return;

    uint32_t width = g_vbe_mode_info.width;
    uint32_t height = g_vbe_mode_info.height;
    uint32_t pitch = g_vbe_mode_info.pitch;
    uint8_t bpp = g_vbe_mode_info.bpp;
    uint32_t base = g_vbe_mode_info.framebuffer;

    if (base == 0U || width == 0U || height == 0U || pitch == 0U || bpp == 0U) {
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

    if ((uint32_t)x == 0U || (uint32_t)y == 0U)
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
