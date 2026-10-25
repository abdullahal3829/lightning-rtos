#ifndef BOOT_PROTOCOL_H
#define BOOT_PROTOCOL_H
#include <stdint.h>

typedef enum VbeMode {
    /* --- 8-bit Color Depth (256 Colors / Indexed Palette) --- */
    VBE_MODE_640x400x256        = 0x100,
    VBE_MODE_640x480x256        = 0x101,
    VBE_MODE_800x600x256        = 0x103,
    VBE_MODE_1024x768x256       = 0x105,
    VBE_MODE_1280x1024x256      = 0x107,

    /* --- 15-bit Color Depth (32K Colors, 5:5:5) --- */
    VBE_MODE_320x200x32K        = 0x10D,
    VBE_MODE_640x480x32K        = 0x110,
    VBE_MODE_800x600x32K        = 0x113,
    VBE_MODE_1024x768x32K       = 0x116,
    VBE_MODE_1280x1024x32K      = 0x119,

    /* --- 16-bit Color Depth (64K Colors, 5:6:5 High Color) --- */
    VBE_MODE_320x200x64K        = 0x10E,
    VBE_MODE_640x480x64K        = 0x111,
    VBE_MODE_800x600x64K        = 0x114,
    VBE_MODE_1024x768x64K       = 0x117,
    VBE_MODE_1280x1024x64K      = 0x11A,

    /* --- 24-bit Color Depth (16.7M Colors, 8:8:8 True Color) --- */
    VBE_MODE_320x200x24         = 0x10F,
    VBE_MODE_640x480x24         = 0x112,
    VBE_MODE_800x600x24         = 0x115,
    VBE_MODE_1024x768x24        = 0x118,
    VBE_MODE_1280x1024x24       = 0x11B,

    /* --- 32-bit Color Depth (16.7M Colors + Alpha/Padding, True Color) --- */
    /* Note: In older VBE 1.2 implementations, 10D/10E/10F were heavily mixed. */
    /* 0x120+ are commonly assigned for modern 32-bit color depths.             */
    VBE_MODE_640x480x32         = 0x121,
    VBE_MODE_800x600x32         = 0x122,
    VBE_MODE_1024x768x32        = 0x123,
    VBE_MODE_1280x1024x32       = 0x124,

    /* --- Text Modes Extended --- */
    VBE_MODE_TEXT_80x60         = 0x108,
    VBE_MODE_TEXT_132x25        = 0x109,
    VBE_MODE_TEXT_132x43        = 0x10A,
    VBE_MODE_TEXT_132x50        = 0x10B,
    VBE_MODE_TEXT_132x60        = 0x10C,
} VbeMode;

typedef struct __attribute__((packed)) VbeInfoBlock {
   char     VbeSignature[4];         // == "VESA"
   uint16_t VbeVersion;              // == 0x0300 for VBE 3.0
   uint16_t OemStringPtr[2];         // isa vbeFarPtr
   uint8_t  Capabilities[4];
   uint16_t VideoModePtr[2];         // isa vbeFarPtr
   uint16_t TotalMemory;             // as # of 64KB blocks
   uint8_t  Reserved[492];
} VbeInfoBlock;

struct vbe_mode_info_structure {
	uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
	uint8_t window_a;			// deprecated
	uint8_t window_b;			// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;			// height in pixels
	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes;
	uint8_t bpp;			// bits per pixel in this mode
	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model;
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages;
	uint8_t reserved0;

	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;

	uint32_t framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
	uint8_t reserved1[206];
} __attribute__ ((packed));

#endif // BOOT_PROTOCOL_H
