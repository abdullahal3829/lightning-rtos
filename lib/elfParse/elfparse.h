/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef ELFPARSE_H
#define ELFPARSE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <varutil.h>

typedef enum {
    ELF_STAGE1,
    ELF_STAGE2,
    ELF_STAGE3,
    ELF_STAGE4,
    ELF_STAGE5
} ELFLoadStages;

typedef struct {
    vector headers;     // Program Header
    void *header;       // Main Metadata Header
    ELFLoadStages stage;
    int errorCode;
    int bit;
    uint32_t sectionIndex;
    uint32_t symbolTableIndex;
    uint32_t symbolEntryIndex;
    bool symbolTableActive;
    bool symbolsDone;
    bool hasMagic;
    bool hasRevision;
    bool isDone;
} VarELF;

/* --- 32-bit ELF Header (Core Fields Only) --- */
typedef struct __attribute__((packed)) {
    uint32_t      e_entry;       // Program entry virtual address
    uint32_t      e_phoff;       // Program header table file offset
    uint32_t      e_shoff;       // Section header table file offset
    uint16_t      e_ehsize;      // ELF header size in bytes
    uint16_t      e_phentsize;   // Program header entry size
    uint16_t      e_phnum;       // Number of program header entries
    uint16_t      e_shentsize;   // Section header entry size
    uint16_t      e_shnum;       // Number of section header entries
    uint16_t      e_shstrndx;    // Section header string table index
} Elf32_Ehdr;

/* --- 64-bit ELF Header (Core Fields Only) --- */
typedef struct __attribute__((packed)) {
    uint64_t      e_entry;       // Program entry virtual address
    uint64_t      e_phoff;       // Program header table file offset
    uint64_t      e_shoff;       // Section header table file offset
    uint16_t      e_ehsize;      // ELF header size in bytes
    uint16_t      e_phentsize;   // Program header entry size
    uint16_t      e_phnum;       // Number of program header entries
    uint16_t      e_shentsize;   // Section header entry size
    uint16_t      e_shnum;       // Number of section header entries
    uint16_t      e_shstrndx;    // Section header string table index
} Elf64_Ehdr;

/* --- Section Header --- */
typedef struct __attribute__((packed)) {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} Elf32_Shdr;

typedef struct __attribute__((packed)) {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

/* --- Symbol table entries --- */
typedef struct __attribute__((packed)) {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
} Elf32_Sym;

typedef struct __attribute__((packed)) {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

/* --- 32-bit Program Header --- */
typedef struct __attribute__((packed)) {
    uint32_t p_type;     // Type of segment
    uint32_t p_offset;   // Offset in the file
    uint32_t p_vaddr;    // Virtual memory address
    uint32_t p_paddr;    // Physical address
    uint32_t p_filesz;   // Size in the file
    uint32_t p_memsz;    // Size in memory
    uint32_t p_flags;    // Segment flags
    uint32_t p_align;    // Required alignment
} Elf32_Phdr;

/* --- 64-bit Program Header --- */
typedef struct __attribute__((packed)) {
    uint32_t p_type;     // Type of segment
    uint32_t p_flags;    // Segment flags
    uint64_t p_offset;   // Offset in the file
    uint64_t p_vaddr;    // Virtual memory address
    uint64_t p_paddr;    // Physical address
    uint64_t p_filesz;   // Size in the file
    uint64_t p_memsz;    // Size in memory
    uint64_t p_align;    // Required alignment
} Elf64_Phdr;

/* --- Generic Unified Program Header Struct --- */
typedef struct ElfProgramHeader {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset; // Offset of data in elf image
  uint64_t p_vaddr;  // Virtual load address
  uint64_t p_paddr;  // Physical load address, not used
  uint64_t p_filesz; // Size of data in elf image
  uint64_t p_memsz;  // Size of data in memory; any excess over disk size is zero'd
  uint64_t p_align;  // Alignment
} ElfProgramHeader;

typedef int (*ELFReadCallback)(void *context, uint64_t offset, void *buffer, size_t size);
extern unsigned int parseELF(ELFReadCallback callback, void *context, size_t length, VarELF *elf);

#endif // ELFPARSE_H