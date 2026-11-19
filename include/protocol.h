/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef BOOT_PROTOCOL_H
#define BOOT_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#else
#define BIOS_BUILD 1
#ifndef __ASSEMBLER__
typedef void *EFI_HANDLE;
typedef struct EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;
typedef struct EFI_GRAPHICS_OUTPUT_MODE_INFORMATION EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;
#endif
#endif

#ifndef __ASSEMBLER__
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

typedef struct {
    uint64_t base;
    unsigned long long size;
} PartitionRegion;

typedef struct __attribute__((packed)) {
    char signature[12];
    uint64_t base;          // Offset 12, size 8
    uint64_t region;        // Offset 20, size 8
    uint32_t type;          // Offset 28, size 4
    uint32_t eattributes;   // Offset 32, size 4 (optional/reserved)
} CommonMemoryEntry;

typedef enum {
    MREGION_USEABLE             = 1,
    MREGION_RESERVED            = 2,
    MREGION_APCI_USEABLE        = 3,
    MREGION_ACPI_NVS            = 4,
    MREGION_BAD                 = 5,
    MREGION_LOADER_RSV          = 6
} MMAPType;
#endif // __ASSEMBLER__

#define SYSCALL_NDISK_READ            0x00000001
#define SYSCALL_NDISK_WRITE           0x00000002
#define SYSCALL_REGISTER_PSTYLE       0x00000003
#define SYSCALL_GET_PARTITIONS_COUNT  0x00000004
#define SYSCALL_GET_PARTITION_ADDRESS 0x00000005
#define SYSCALL_CREATE_PR_PARTITION   0x00000006
#define SYSCALL_RESIZE_PR_PARTITION   0x00000007
#define SYSCALL_DELETE_PR_PARTITION   0x00000008
#define SYSCALL_GET_MEMORY_MAP        0x00000009
#define SYSCALL_SET_MAX_RESULATION    0x0000000A
#define SYSCALL_SET_VBE_MODE          0x0000000C
#define SYSCALL_CALL32                0x0000000D
#define SYSCALL_CALL16                0x0000000E
#define SYSCALL_LIALLOCATE_MEMORY     0x00000010
#define SYSCALL_LIREALLOCATE_MEMORY   0x00000011
#define SYSCALL_LIFREE_MEMORY         0x00000012
#define SYSCALL_LIRESERVE_MEMORY      0x00000013
#define SYSCALL_LIUNRESERVE_MEMORY    0x00000014
#define SYSCALL_SERIAL_WRITE          0x00000015
#define SYSCALL_SERIAL_READ           0x00000016
#define SYSCALL_SERIAL_READB          0x00000017
#define SYSCALL_NACPI_POWER_OFF       0x00000018
#define SYSCALL_NACPI_REBOOT          0x00000019

#ifndef __ASSEMBLER__
typedef enum {
    NDISK_READ                  = SYSCALL_NDISK_READ,
    NDISK_WRITE                 = SYSCALL_NDISK_WRITE,
    REGISTER_PSTYLE             = SYSCALL_REGISTER_PSTYLE,
    GET_PARTITIONS_COUNT        = SYSCALL_GET_PARTITIONS_COUNT,
    GET_PARTITION_ADDRESS       = SYSCALL_GET_PARTITION_ADDRESS,
    CREATE_PR_PARTITION         = SYSCALL_CREATE_PR_PARTITION,
    RESIZE_PR_PARTITION         = SYSCALL_RESIZE_PR_PARTITION,
    DELETE_PR_PARTITION         = SYSCALL_DELETE_PR_PARTITION,
    GET_MEMORY_MAP              = SYSCALL_GET_MEMORY_MAP,
    SET_VBE_MODE                = SYSCALL_SET_VBE_MODE,
    CALL16                      = SYSCALL_CALL16,
    CALL32                      = SYSCALL_CALL32,
    LIALLOCATE_MEMORY           = SYSCALL_LIALLOCATE_MEMORY,
    LIREALLOCATE_MEMORY         = SYSCALL_LIREALLOCATE_MEMORY,
    LIFREE_MEMORY               = SYSCALL_LIFREE_MEMORY,
    LIRESERVE_MEMORY            = SYSCALL_LIRESERVE_MEMORY,
    LIUNRESERVE_MEMORY          = SYSCALL_LIUNRESERVE_MEMORY,
    SERIAL_WRITE                = SYSCALL_SERIAL_WRITE,
    SERIAL_READ                 = SYSCALL_SERIAL_READ,
    SERIAL_READB                = SYSCALL_SERIAL_READB,
    NACPI_POWER_OFF             = SYSCALL_NACPI_POWER_OFF,
    NACPI_REBOOT                = SYSCALL_NACPI_REBOOT
} SyscallCode;

typedef enum {
    BIOS = 0,
    UEFI = 1
} RunningFirmware;

/*
 * Don't Confuse With EFI Boot Protocol It Is Extended Reuseable
 * Boot Protocol But It Created For Functions Reuse,
 * Instead Of Abandoning 60-70% Bootloader's Functions.
 */
typedef struct __attribute__((packed)) {
    void* handleSyscall;
    VbeInfoBlock* vbeInfo;
#if !defined(EFI_BUILD)
    struct vbe_mode_info_structure* vbeModeInfo;
    VbeMode currentMode;
#else
    char* padding[sizeof(struct vbe_mode_info_structure) + sizeof(VbeMode)];
#endif
    RunningFirmware firmware;
#if defined(EFI_BUILD)
    EFI_HANDLE ImageHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    unsigned int SizeOfInfo;
    uint32_t TargetMode;
    bool ModeFound;
#endif
} EBootProtocol;

#endif // __ASSEMBLER__
#endif // BOOT_PROTOCOL_H