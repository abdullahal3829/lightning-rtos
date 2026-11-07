/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <varutil.h>
#include <definations.h>
#include "elfparse.h"

#ifdef BOOTLDR_BUILD
#include <mm.h>
#else
#include <mm/mm.h>
#endif

static unsigned int readELFByte(ELFReadCallback callback, void *context, size_t length, uint64_t offset, char *buffer) {
    if (callback == 0 || buffer == (char *)0)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
    if (offset >= length)
        return SYSTEM_CODE_ERROR_INVALID_SEEK;
    unsigned int result = (unsigned int)callback(context, offset, buffer, 1U);
    return result;
}

static unsigned int readBytes(ELFReadCallback callback, void *context, size_t length, uint64_t offset, void *dest, size_t count) {
    if (callback == 0 || dest == (void *)0)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
    if (offset > length || count > length - (size_t)offset)
        return SYSTEM_CODE_ERROR_INVALID_SEEK;
    return (unsigned int)callback(context, offset, dest, count);
}

static int stringEquals(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++)
            return 0;
    }
    return *left == *right;
}

static int validateKernelSymbol(ELFReadCallback callback, void *context, size_t length,
                                VarELF *elf, uint64_t value, uint64_t size,
                                uint8_t info, uint16_t sectionIndex,
                                uint64_t sectionAddress, uint64_t sectionSize,
                                uint64_t sectionFlags, uint64_t expectedSize,
                                uint32_t expectedValue) {
    if ((info >> 4) != 1U || (info & 0x0FU) != 1U || size != expectedSize ||
        sectionIndex == 0U || value < sectionAddress ||
        value - sectionAddress > sectionSize || size > sectionSize - (value - sectionAddress))
        return SYSTEM_CODE_INVALID_BINARY;

    for (uint32_t i = 0; i < elf->headers.count; ++i) {
        ElfProgramHeader *program = (ElfProgramHeader *)vectorAt(&elf->headers, i);
        if (program == NULL)
            return SYSTEM_CODE_INVALID_BINARY;
        uint64_t fileOffset;
        uint32_t storedValue = 0U;
        if (program->p_type != 1U || value < program->p_vaddr ||
            value - program->p_vaddr > program->p_filesz ||
            size > program->p_filesz - (value - program->p_vaddr))
            continue;
        if (program->p_offset > length || value - program->p_vaddr > length - program->p_offset)
            return SYSTEM_CODE_INVALID_BINARY;
        fileOffset = program->p_offset + value - program->p_vaddr;
        if (readBytes(callback, context, length, fileOffset, &storedValue, (size_t)size) != SYSTEM_CODE_SUCCESS)
            return SYSTEM_CODE_INVALID_BINARY;
        return storedValue == expectedValue ? SYSTEM_CODE_SUCCESS : SYSTEM_CODE_INVALID_BINARY;
    }
    return SYSTEM_CODE_INVALID_BINARY;
}

typedef struct {
    uint32_t type;
    uint64_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint64_t entrySize;
} ELFSectionInfo;

typedef struct {
    uint32_t name;
    uint64_t value;
    uint64_t size;
    uint8_t info;
    uint16_t sectionIndex;
} ELFSymbolInfo;

static unsigned int readSectionInfo(ELFReadCallback callback, void *context, size_t length,
                                    VarELF *elf, uint32_t index, ELFSectionInfo *section) {
    uint64_t tableOffset;
    uint16_t count;
    uint16_t entrySize;
    if (elf->bit == 1) {
        Elf32_Ehdr *header = (Elf32_Ehdr *)elf->header;
        tableOffset = header->e_shoff;
        count = header->e_shnum;
        entrySize = header->e_shentsize;
        if (entrySize != sizeof(Elf32_Shdr) || index >= count || tableOffset > length ||
            (uint64_t)count * entrySize > length - tableOffset)
            return SYSTEM_CODE_INVALID_BINARY;
        Elf32_Shdr raw;
        unsigned int result = readBytes(callback, context, length,
            tableOffset + (uint64_t)index * entrySize, &raw, sizeof(raw));
        if (result != SYSTEM_CODE_SUCCESS)
            return result;
        section->type = raw.sh_type;
        section->flags = raw.sh_flags;
        section->address = raw.sh_addr;
        section->offset = raw.sh_offset;
        section->size = raw.sh_size;
        section->link = raw.sh_link;
        section->entrySize = raw.sh_entsize;
    } else if (elf->bit == 2) {
        Elf64_Ehdr *header = (Elf64_Ehdr *)elf->header;
        tableOffset = header->e_shoff;
        count = header->e_shnum;
        entrySize = header->e_shentsize;
        if (entrySize != sizeof(Elf64_Shdr) || index >= count || tableOffset > length ||
            (uint64_t)count * entrySize > length - tableOffset)
            return SYSTEM_CODE_INVALID_BINARY;
        Elf64_Shdr raw;
        unsigned int result = readBytes(callback, context, length,
            tableOffset + (uint64_t)index * entrySize, &raw, sizeof(raw));
        if (result != SYSTEM_CODE_SUCCESS)
            return result;
        section->type = raw.sh_type;
        section->flags = raw.sh_flags;
        section->address = raw.sh_addr;
        section->offset = raw.sh_offset;
        section->size = raw.sh_size;
        section->link = raw.sh_link;
        section->entrySize = raw.sh_entsize;
    } else {
        return SYSTEM_CODE_INVALID_BINARY;
    }
    return SYSTEM_CODE_SUCCESS;
}

static unsigned int readSymbolInfo(ELFReadCallback callback, void *context, size_t length,
                                   VarELF *elf, const ELFSectionInfo *table,
                                   uint32_t index, ELFSymbolInfo *symbol) {
    ELFSectionInfo strings;
    if (table->entrySize == 0U || table->link > UINT32_MAX ||
        readSectionInfo(callback, context, length, elf, (uint32_t)table->link, &strings) != SYSTEM_CODE_SUCCESS ||
        strings.type != 3U || index >= table->size / table->entrySize ||
        table->offset > length || table->size > (uint64_t)length - table->offset ||
        strings.offset > length || strings.size > (uint64_t)length - strings.offset)
        return SYSTEM_CODE_INVALID_BINARY;

    if (elf->bit == 1) {
        Elf32_Sym raw;
        if (table->entrySize != sizeof(raw) ||
            readBytes(callback, context, length, table->offset + (uint64_t)index * table->entrySize,
                      &raw, sizeof(raw)) != SYSTEM_CODE_SUCCESS)
            return SYSTEM_CODE_INVALID_BINARY;
        symbol->name = raw.st_name;
        symbol->value = raw.st_value;
        symbol->size = raw.st_size;
        symbol->info = raw.st_info;
        symbol->sectionIndex = raw.st_shndx;
    } else {
        Elf64_Sym raw;
        if (table->entrySize != sizeof(raw) ||
            readBytes(callback, context, length, table->offset + (uint64_t)index * table->entrySize,
                      &raw, sizeof(raw)) != SYSTEM_CODE_SUCCESS)
            return SYSTEM_CODE_INVALID_BINARY;
        symbol->name = raw.st_name;
        symbol->value = raw.st_value;
        symbol->size = raw.st_size;
        symbol->info = raw.st_info;
        symbol->sectionIndex = raw.st_shndx;
    }
    if (symbol->name >= strings.size)
        return SYSTEM_CODE_INVALID_BINARY;
    return SYSTEM_CODE_SUCCESS;
}

static int validateNextKernelSymbol(ELFReadCallback callback, void *context,
                                    size_t length, VarELF *elf) {
    uint16_t sectionCount = elf->bit == 1 ?
        ((Elf32_Ehdr *)elf->header)->e_shnum : ((Elf64_Ehdr *)elf->header)->e_shnum;

    if (elf->symbolsDone)
        return SYSTEM_CODE_SUCCESS;

    while (elf->sectionIndex < sectionCount) {
        ELFSectionInfo table;
        unsigned int result;
        if (!elf->symbolTableActive) {
            result = readSectionInfo(callback, context, length, elf, elf->sectionIndex, &table);
            if (result != SYSTEM_CODE_SUCCESS)
                return (int)result;
            if (table.type != 2U && table.type != 11U) {
                ++elf->sectionIndex;
                continue;
            }
            if (table.entrySize != (elf->bit == 1 ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym)) ||
                table.link >= sectionCount || table.offset > length ||
                table.size > (uint64_t)length - table.offset)
                return SYSTEM_CODE_INVALID_BINARY;
            elf->symbolTableIndex = elf->sectionIndex;
            elf->symbolEntryIndex = 0U;
            elf->symbolTableActive = true;
        }

        result = readSectionInfo(callback, context, length, elf, elf->symbolTableIndex, &table);
        if (result != SYSTEM_CODE_SUCCESS)
            return (int)result;
        uint64_t symbolCount = table.size / table.entrySize;
        if (elf->symbolEntryIndex >= symbolCount) {
            elf->sectionIndex = elf->symbolTableIndex + 1U;
            elf->symbolTableActive = false;
            continue;
        }

        ELFSectionInfo strings;
        result = readSectionInfo(callback, context, length, elf, table.link, &strings);
        if (result != SYSTEM_CODE_SUCCESS)
            return (int)result;
        ELFSymbolInfo symbol;
        result = readSymbolInfo(callback, context, length, elf, &table,
                                elf->symbolEntryIndex, &symbol);
        if (result != SYSTEM_CODE_SUCCESS)
            return (int)result;
        char name[32] = {0};
        uint64_t nameOffset = strings.offset + symbol.name;
        for (size_t i = 0; i < sizeof(name) - 1U &&
             (uint64_t)symbol.name + i < strings.size; ++i) {
            result = readELFByte(callback, context, length, nameOffset + i, &name[i]);
            if (result != SYSTEM_CODE_SUCCESS)
                return (int)result;
            if (name[i] == '\0')
                break;
        }
        ++elf->symbolEntryIndex;

        if (stringEquals(name, "EB_MAGIC") || stringEquals(name, "EB_RIVISON")) {
            ELFSectionInfo symbolSection;
            result = readSectionInfo(callback, context, length, elf, symbol.sectionIndex, &symbolSection);
            if (result != SYSTEM_CODE_SUCCESS)
                return (int)result;
            if (stringEquals(name, "EB_MAGIC")) {
                if (elf->hasMagic || validateKernelSymbol(callback, context, length, elf,
                    symbol.value, symbol.size, symbol.info, symbol.sectionIndex,
                    symbolSection.address, symbolSection.size, symbolSection.flags,
                    1U, 0xAEU) != SYSTEM_CODE_SUCCESS)
                    return SYSTEM_CODE_INVALID_BINARY;
                elf->hasMagic = true;
            } else {
                if (elf->hasRevision || validateKernelSymbol(callback, context, length, elf,
                    symbol.value, symbol.size, symbol.info, symbol.sectionIndex,
                    symbolSection.address, symbolSection.size, symbolSection.flags,
                    4U, 1U) != SYSTEM_CODE_SUCCESS)
                    return SYSTEM_CODE_INVALID_BINARY;
                elf->hasRevision = true;
            }
        }
        return SYSTEM_CODE_SUCCESS;
    }

    elf->symbolsDone = true;
    return elf->hasMagic && elf->hasRevision ? SYSTEM_CODE_SUCCESS : SYSTEM_CODE_INVALID_BINARY;
}

unsigned int parseELF(ELFReadCallback callback, void *context, size_t length, VarELF *elf) {
    if (elf == (VarELF *)0 || callback == 0)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
    if (length == 0U)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;

    if (elf->header == (void *)0) {
        elf->stage = ELF_STAGE1;
        elf->errorCode = 0;

        uint8_t buffer[5] = {0};
        unsigned int result = readELFByte(callback, context, length, 0, (char *)&buffer[0]);
        const char *signature = "ELF";
        if (result != SYSTEM_CODE_SUCCESS) {
        dskErrWELFLoad:
            elf->errorCode = result;
            goto elfError;
        }
        if (buffer[0] == 0x7F) {
            for (int i = 0; i < 3; i++) {
                result = readELFByte(callback, context, length, (uint64_t)i + 1U, (char *)&buffer[i + 1]);
                if (result != SYSTEM_CODE_SUCCESS)
                    return result;
                else if (buffer[i + 1] != (uint8_t)signature[i])
                    goto invBIN;
            }
        } else
            goto invBIN;

#ifdef BOOTLDR_BUILD
        result = readELFByte(callback, context, length, 4, (char *)&buffer[4]);
        if (result != SYSTEM_CODE_SUCCESS)
            goto dskErrWELFLoad;

        elf->bit = (int)buffer[4];
        if ((int)buffer[4] == 1) {
            elf->header = falloc(sizeof(Elf32_Ehdr));
            if (elf->header == (void *)0)
                goto elfoom;
            goto end;
        } else if ((int)buffer[4] == 2) {
            elf->header = falloc(sizeof(Elf64_Ehdr));
            if (elf->header == (void *)0)
                goto elfoom;
            goto end;
        } else {
            goto invBIN;
        }
#else
        elf->header = kmalloc(sizeof(Elf64_Ehdr));
        if (elf->header == (void *)0)
            goto elfoom;
#endif
    }

    elf->isDone = false;
    switch (elf->stage) {
        case ELF_STAGE1:
            uint8_t buffer = 0;
            unsigned int result = readELFByte(callback, context, length, 0, (char *)&buffer);
            if (result != SYSTEM_CODE_SUCCESS)
                goto dskErrWELFLoad;

            if (buffer != 0x7F) {
                elf->errorCode = SYSTEM_CODE_INVALID_BINARY;
                goto elfError;
            }
#ifdef BOOTLDR_BUILD
            uint8_t elfClass = 0;
            uint8_t elfData = 0;
            uint8_t identVersion = 0;
            if (readELFByte(callback, context, length, 4, (char *)&elfClass) != SYSTEM_CODE_SUCCESS ||
                readELFByte(callback, context, length, 5, (char *)&elfData) != SYSTEM_CODE_SUCCESS ||
                readELFByte(callback, context, length, 6, (char *)&identVersion) != SYSTEM_CODE_SUCCESS ||
                elfClass != (uint8_t)elf->bit || elfData != 1U || identVersion != 1U)
                goto invBIN;
            elf->stage = ELF_STAGE3;
#else
            elf->stage = ELF_STAGE2;
#endif
            break;

#ifndef BOOTLDR_BUILD
        case ELF_STAGE2:
            uint8_t tmp[7] = {0};
            for (int i = 0; i < 7; i++) {
                unsigned int result2 = readELFByte(callback, context, length, 8 + i, (char *)&tmp[i]);
                if (result2 != SYSTEM_CODE_SUCCESS)
                    goto dskErrWELFLoad;
            }

            if ((unsigned int)tmp[0] > 0xFFU)
                goto elfIncap;

            unsigned int result2 = readELFByte(callback, context, length, 18, (char *)&tmp[0]);
            if (result2 != SYSTEM_CODE_SUCCESS)
                goto dskErrWELFLoad;

            if (tmp[0] != 0xFF)
                goto elfIncap;

            result2 = readELFByte(callback, context, length, 18, (char *)&tmp[0]);
            if (result2 != SYSTEM_CODE_SUCCESS)
                goto dskErrWELFLoad;
            break;
#else
        case ELF_STAGE2:
            elf->stage = ELF_STAGE3;
            break;
#endif

        case ELF_STAGE3:
            uint16_t machine = 0;
            if (readBytes(callback, context, length, 18, &machine, sizeof(machine)) != SYSTEM_CODE_SUCCESS)
                goto dskErrWELFLoad;
            if ((elf->bit == 1 && machine != 3U) || (elf->bit == 2 && machine != 62U))
                goto elfIncap;
#ifdef BOOTLDR_BUILD
            if (elf->bit == 1) {
                char *dest = (char *)elf->header + OFFSETOF(Elf32_Ehdr, e_entry);
                for (int i = 0; i < (int)(STSIZEOF(Elf32_Ehdr, e_entry) + STSIZEOF(Elf32_Ehdr, e_phoff) +
                                          STSIZEOF(Elf32_Ehdr, e_shoff)); i++) {
                    unsigned int result3 = readELFByte(callback, context, length, 24 + i, dest + i);
                    if (result3 != SYSTEM_CODE_SUCCESS)
                        goto dskErrWELFLoad;
                }
            } else if (elf->bit == 2) {
#endif
                char *dest = (char *)elf->header + OFFSETOF(Elf64_Ehdr, e_entry);
                for (int i = 0; i < (int)(STSIZEOF(Elf64_Ehdr, e_entry) + STSIZEOF(Elf64_Ehdr, e_phoff) +
                                          STSIZEOF(Elf64_Ehdr, e_shoff)); i++) {
                    unsigned int result3 = readELFByte(callback, context, length, 24 + i, dest + i);
                    if (result3 != SYSTEM_CODE_SUCCESS)
                        goto dskErrWELFLoad;
                }
#ifdef BOOTLDR_BUILD
            }
#endif
            elf->stage = ELF_STAGE4;
            break;

        case ELF_STAGE4:
#ifdef BOOTLDR_BUILD
            if (elf->bit == 1) {
                char *dest = (char *)elf->header + OFFSETOF(Elf32_Ehdr, e_ehsize);
                for (int i = 0; i < (int)(STSIZEOF(Elf32_Ehdr, e_ehsize) + STSIZEOF(Elf32_Ehdr, e_phnum) +
                                          STSIZEOF(Elf32_Ehdr, e_shentsize) + STSIZEOF(Elf32_Ehdr, e_shnum) +
                                          STSIZEOF(Elf32_Ehdr, e_shstrndx)); i++) {
                    unsigned int result4 = readELFByte(callback, context, length, 40 + i, dest + i);
                    if (result4 != SYSTEM_CODE_SUCCESS)
                        goto dskErrWELFLoad;
                }
            } else if (elf->bit == 2) {
#endif
                char *dest = (char *)elf->header + OFFSETOF(Elf64_Ehdr, e_ehsize);
                for (int i = 0; i < (int)(STSIZEOF(Elf64_Ehdr, e_ehsize) + STSIZEOF(Elf64_Ehdr, e_phnum) +
                                          STSIZEOF(Elf64_Ehdr, e_shentsize) + STSIZEOF(Elf64_Ehdr, e_shnum) +
                                          STSIZEOF(Elf64_Ehdr, e_shstrndx)); i++) {
                    unsigned int result4 = readELFByte(callback, context, length, 52 + i, dest + i);
                    if (result4 != SYSTEM_CODE_SUCCESS)
                        goto dskErrWELFLoad;
                }
#ifdef BOOTLDR_BUILD
            }
#endif
            elf->stage = ELF_STAGE5;
            break;

        case ELF_STAGE5:
#ifdef BOOTLDR_BUILD
            if (elf->bit == 1) {
                Elf32_Ehdr *header = (Elf32_Ehdr *)elf->header;
                if (header->e_phentsize != sizeof(Elf32_Phdr) ||
                    header->e_phnum == 0U ||
                    header->e_phoff > length ||
                    (uint64_t)header->e_phnum * header->e_phentsize > length - header->e_phoff)
                    goto invBIN;

                if (elf->headers.capacity == 0U) {
                    vectorInit(&elf->headers);
                    if (!vectorAllocate(&elf->headers, header->e_phnum, sizeof(ElfProgramHeader)))
                        goto elfoom;
                }

                if (elf->headers.count < header->e_phnum) {
                    Elf32_Phdr programHeader;
                    char *dest = (char *)&programHeader;
                    uint64_t offset = (uint64_t)header->e_phoff +
                                      (uint64_t)elf->headers.count * header->e_phentsize;
                    for (size_t i = 0; i < sizeof(programHeader); i++) {
                        unsigned int result5 = readELFByte(callback, context, length, offset + i, &dest[i]);
                        if (result5 != SYSTEM_CODE_SUCCESS)
                            goto dskErrWELFLoad;
                    }
                    ElfProgramHeader parsedHeader = {
                        .p_type = programHeader.p_type,
                        .p_offset = programHeader.p_offset,
                        .p_vaddr = programHeader.p_vaddr,
                        .p_paddr = programHeader.p_paddr,
                        .p_filesz = programHeader.p_filesz,
                        .p_memsz = programHeader.p_memsz,
                        .p_flags = programHeader.p_flags,
                        .p_align = programHeader.p_align
                    };
                    if (!vectorAppend(&elf->headers, &parsedHeader, sizeof(parsedHeader)))
                        goto elfoom;
                }
                if (elf->headers.count >= header->e_phnum) {
                    if (validateNextKernelSymbol(callback, context, length, elf) != SYSTEM_CODE_SUCCESS)
                        goto invBIN;
                    elf->isDone = elf->symbolsDone;
                }
                break;
            } else if (elf->bit == 2) {
#endif
                Elf64_Ehdr *header = (Elf64_Ehdr *)elf->header;
                if (header->e_phentsize != sizeof(Elf64_Phdr) ||
                    header->e_phnum == 0U ||
                    header->e_phoff > length ||
                    (uint64_t)header->e_phnum * header->e_phentsize > length - header->e_phoff)
                    goto invBIN;

                if (elf->headers.capacity == 0U) {
                    vectorInit(&elf->headers);
                    if (!vectorAllocate(&elf->headers, header->e_phnum, sizeof(ElfProgramHeader)))
                        goto elfoom;
                }

                if (elf->headers.count < header->e_phnum) {
                    Elf64_Phdr programHeader;
                    char *dest = (char *)&programHeader;
                    uint64_t offset = header->e_phoff +
                                      (uint64_t)elf->headers.count * header->e_phentsize;
                    for (size_t i = 0; i < sizeof(programHeader); i++) {
                        unsigned int result5 = readELFByte(callback, context, length, offset + i, &dest[i]);
                        if (result5 != SYSTEM_CODE_SUCCESS)
                            goto dskErrWELFLoad;
                    }
                    ElfProgramHeader parsedHeader = {
                        .p_type = programHeader.p_type,
                        .p_offset = programHeader.p_offset,
                        .p_vaddr = programHeader.p_vaddr,
                        .p_paddr = programHeader.p_paddr,
                        .p_filesz = programHeader.p_filesz,
                        .p_memsz = programHeader.p_memsz,
                        .p_flags = programHeader.p_flags,
                        .p_align = programHeader.p_align
                    };
                    if (!vectorAppend(&elf->headers, &parsedHeader, sizeof(parsedHeader)))
                        goto elfoom;
                }
                if (elf->headers.count >= header->e_phnum) {
#ifdef BOOTLDR_BUILD
                    if (validateNextKernelSymbol(callback, context, length, elf) != SYSTEM_CODE_SUCCESS)
                        goto invBIN;
#endif
                    elf->isDone = elf->symbolsDone;
                }
                break;
#ifdef BOOTLDR_BUILD
            }
#endif
    }

end:
    return SYSTEM_CODE_SUCCESS;
invBIN:
    elf->errorCode = SYSTEM_CODE_INVALID_BINARY;
    goto elfError;
elfIncap:
    elf->errorCode = SYSTEM_CODE_ERROR_INCAPABLE;
    goto elfError;
elfoom:
    elf->errorCode = SYSTEM_CODE_ERROR_OUT_OF_MEMORY;
elfError:
    return elf->errorCode;
}