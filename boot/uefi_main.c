/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 *
 * UEFI 1st-stage bootloader (minimal).
 */

#include <Uefi.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>

/* ---------------------------------------------------------------- *
 *  UEFI GUID definitions                                           *
 * ---------------------------------------------------------------- */
EFI_GUID gEfiSimpleFileSystemProtocolGuid = { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };

/* ---------------------------------------------------------------- *
 *  UEFI globals                                                    *
 * ---------------------------------------------------------------- */
static EFI_SYSTEM_TABLE   *gST;
static EFI_BOOT_SERVICES  *gBS;
static EFI_HANDLE          gImageHandle;

EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5B1B31A1, 0x9562, 0x11D2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* ---------------------------------------------------------------- *
 *  UEFI console output                                             *
 * ---------------------------------------------------------------- */
static void kprintf(const CHAR16 *s) {
    if (gST != NULL && gST->ConOut != NULL)
        gST->ConOut->OutputString(gST->ConOut, (CHAR16 *)s);
}

static void uefi_hex(uint64_t v) {
    static const char hex[] = "0123456789ABCDEF";
    CHAR16 buf[19];
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 0; i < 16; i++)
        buf[2 + i] = (CHAR16)hex[(v >> (60 - i * 4)) & 0xF];
    buf[18] = 0;
    gST->ConOut->OutputString(gST->ConOut, buf);
}

/* ---------------------------------------------------------------- *
 *  Minimal ELF64 parser / loader                                   *
 * ---------------------------------------------------------------- */
#define ELFCLASS64  2
#define PT_LOAD     1

typedef struct {
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

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

static EFI_STATUS read_file(EFI_FILE_PROTOCOL *file, uint64_t offset,
                              void *buf, UINTN size) {
    EFI_STATUS st;
    UINTN rsize = size;
    st = file->SetPosition(file, offset);
    if (EFI_ERROR(st)) return st;
    st = file->Read(file, &rsize, buf);
    if (EFI_ERROR(st) || rsize != size) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

typedef struct {
    uint64_t handle_syscall;
    uint64_t vbe_info;
    uint64_t vbe_mode_info;
    uint32_t current_mode;
    uint32_t firmware;
    uint64_t image_handle;
    uint64_t system_table;
} __attribute__((packed)) BootProtocol;

static void *alloc_pages_at(uint64_t addr, uint64_t size) {
    EFI_STATUS st;
    EFI_PHYSICAL_ADDRESS pa = (EFI_PHYSICAL_ADDRESS)addr;
    UINTN pages = (size + 0xFFF) / 0x1000;
    st = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &pa);
    if (EFI_ERROR(st)) return NULL;
    return (void *)(uintptr_t)pa;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle,
                           EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS st;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *file = NULL;
    CHAR16 path[] = {0x002F, 0x006F, 0x0073, 0x006B, 0x0072, 0x006E, 0x006C, 0x0000};
    Elf64_Ehdr ehdr;
    Elf64_Phdr *phdr = NULL;
    void *load_addr = NULL;
    BootProtocol bp;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
    UINTN memory_map_size = 64U * 1024U;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
    uint64_t i, max_vaddr = 0;

    gImageHandle = ImageHandle;
    gST   = SystemTable;
    gBS   = SystemTable->BootServices;

    /* Debug: mark entry */
    kprintf((CHAR16 *)L"KUEFI START\r\n");

    /* --- UEFI file system ---------------------------------------- */
    kprintf((CHAR16 *)L"Locate FS...\r\n");
    st = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid,
                             (void **)&loaded_image);
    if (!EFI_ERROR(st)) {
        st = gBS->HandleProtocol(loaded_image->DeviceHandle,
                                 &gEfiSimpleFileSystemProtocolGuid,
                                 (void **)&fs);
    }
    if (EFI_ERROR(st)) {
        st = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid,
                                 NULL, (void **)&fs);
    }
    kprintf((CHAR16 *)L"LocateFS ret=0x");
    uefi_hex(st);
    kprintf((CHAR16 *)L"\r\n");
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: no SFS\r\n");
        goto halt;
    }

    st = fs->OpenVolume(fs, &root);
    kprintf((CHAR16 *)L"OpenVolume ret=0x");
    uefi_hex(st);
    kprintf((CHAR16 *)L"\r\n");
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: open volume\r\n");
        goto halt;
    }

    kprintf((CHAR16 *)L"Open /oskrnl...\r\n");
    st = root->Open(root, &file, path, EFI_FILE_MODE_READ, 0);
    kprintf((CHAR16 *)L"Open ret=0x");
    uefi_hex(st);
    kprintf((CHAR16 *)L"\r\n");
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: open /oskrnl\r\n");
        goto halt;
    }

    /* --- Read ELF header -------------------------------------------- */
    kprintf((CHAR16 *)L"Read ELF hdr...\r\n");
    st = read_file(file, 0, &ehdr, sizeof(ehdr));
    kprintf((CHAR16 *)L"Read ELF ret=0x");
    uefi_hex(st);
    kprintf((CHAR16 *)L"\r\n");
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: read ELF hdr\r\n");
        goto halt;
    }

    if (ehdr.e_ident[0] != 0x7F ||
        ehdr.e_ident[1] != 'E'  ||
        ehdr.e_ident[2] != 'L'  ||
        ehdr.e_ident[3] != 'F') {
        kprintf((CHAR16 *)L"FAIL: not ELF\r\n");
        goto halt;
    }
    if (ehdr.e_ident[4] != 2) {
        kprintf((CHAR16 *)L"FAIL: not ELF64\r\n");
        goto halt;
    }
    if (ehdr.e_phnum == 0) {
        kprintf((CHAR16 *)L"FAIL: no PHDR\r\n");
        goto halt;
    }

    kprintf((CHAR16 *)L"ELF64 OK, entry=0x");
    uefi_hex(ehdr.e_entry);
    kprintf((CHAR16 *)L", phnum=");
    uefi_hex(ehdr.e_phnum);
    kprintf((CHAR16 *)L"\r\n");

    /* --- Read all program headers ----------------------------------- */
    st = gBS->AllocatePool(EfiLoaderData,
                           (UINTN)(ehdr.e_phnum * sizeof(Elf64_Phdr)),
                           (void **)&phdr);
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: alloc PHDR\r\n");
        goto halt;
    }

    st = read_file(file, ehdr.e_phoff, phdr,
                   (UINTN)(ehdr.e_phnum * sizeof(Elf64_Phdr)));
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: read PHDR\r\n");
        goto halt;
    }

    /* --- Compute max vaddr ------------------------------------------ */
    for (i = 0; i < ehdr.e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        if (phdr[i].p_vaddr + phdr[i].p_memsz > max_vaddr)
            max_vaddr = phdr[i].p_vaddr + phdr[i].p_memsz;
    }
    kprintf((CHAR16 *)L"max vaddr=0x");
    uefi_hex(max_vaddr);
    kprintf((CHAR16 *)L"\r\n");

    /* --- Load each PT_LOAD segment ---------------------------------- */
    for (i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr *p = &phdr[i];
        void *dest;

        if (p->p_type != PT_LOAD) continue;
        if (p->p_memsz == 0) continue;

        dest = alloc_pages_at(p->p_vaddr, p->p_memsz);
        if (dest == NULL) {
            kprintf((CHAR16 *)L"FAIL: alloc @0x");
            uefi_hex(p->p_vaddr);
            kprintf((CHAR16 *)L"\r\n");
            goto halt;
        }

        if (p->p_filesz > 0) {
            st = read_file(file, p->p_offset, dest, (UINTN)p->p_filesz);
            if (EFI_ERROR(st)) {
                kprintf((CHAR16 *)L"FAIL: read seg\r\n");
                goto halt;
            }
        }

        if (p->p_memsz > p->p_filesz) {
            uint8_t *z = (uint8_t *)dest + p->p_filesz;
            uint64_t zn = p->p_memsz - p->p_filesz;
            while (zn--) *z++ = 0;
        }

        kprintf((CHAR16 *)L"load @0x");
        uefi_hex(p->p_vaddr);
        kprintf((CHAR16 *)L", memsz=0x");
        uefi_hex(p->p_memsz);
        kprintf((CHAR16 *)L"\r\n");
    }

    load_addr = (void *)(uintptr_t)ehdr.e_entry;
    kprintf((CHAR16 *)L"entry=0x");
    uefi_hex(ehdr.e_entry);
    kprintf((CHAR16 *)L"\r\n");

    file->Close(file);
    file = NULL;

    bp.handle_syscall = 0;
    bp.vbe_info = 0;
    bp.vbe_mode_info = 0;
    bp.current_mode = 0;
    bp.firmware = 1;
    bp.image_handle = (uint64_t)(uintptr_t)ImageHandle;
    bp.system_table = (uint64_t)(uintptr_t)gST;

    kprintf((CHAR16 *)L"Chainloaded\r\n");

    st = gBS->AllocatePool(EfiLoaderData, memory_map_size, (void **)&memory_map);
    if (EFI_ERROR(st)) {
        kprintf((CHAR16 *)L"FAIL: alloc memory map\r\n");
        goto halt;
    }

    /* GetMemoryMap can change the key, so call ExitBootServices immediately
     * after each successful map query, without printing or allocating. */
    for (;;) {
        st = gBS->GetMemoryMap(&memory_map_size, memory_map, &map_key,
                              &descriptor_size, &descriptor_version);
        if (st == EFI_BUFFER_TOO_SMALL) {
            gBS->FreePool(memory_map);
            memory_map = NULL;
            memory_map_size += 8U * (descriptor_size != 0U ? descriptor_size : 64U);
            st = gBS->AllocatePool(EfiLoaderData, memory_map_size,
                                   (void **)&memory_map);
            if (EFI_ERROR(st))
                goto halt;
            continue;
        }
        if (EFI_ERROR(st))
            goto halt;
        st = gBS->ExitBootServices(ImageHandle, map_key);
        if (!EFI_ERROR(st))
            break;
    }

    /* --- Transfer control to kernel --------------------------------- */
    __asm__ volatile (
        "mov %0, %%rdi\n"
        "call *%1\n"
        :
        : "r"(&bp), "r"(load_addr)
        : "rdi", "memory"
    );

halt:
    kprintf((CHAR16 *)L"HALT\r\n");
    for (;;)
        __asm__ volatile ("cli; hlt");

    return EFI_LOAD_ERROR;
}
