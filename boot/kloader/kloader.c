/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdint.h>
#include <stddef.h>
#include <definations.h>
#include "../mm.h"
#include "kloader.h"

static void copyBytes(uint8_t *destination, const uint8_t *source, size_t count) {
    for (size_t i = 0; i < count; ++i)
        destination[i] = source[i];
}

static void clearBytes(uint8_t *destination, size_t count) {
    for (size_t i = 0; i < count; ++i)
        destination[i] = 0U;
}

int loadKernel(const KernelFile *file, KernelImage *image) {
    VarELF kelf = {0};
    if (file == NULL || image == NULL || file->read == NULL || file->size == 0U ||
        file->size > (uint64_t)(size_t)-1)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
    size_t length = (size_t)file->size;

    while (!kelf.isDone) {
        ELFLoadStages previousStage = kelf.stage;
        uint32_t previousHeaderCount = kelf.headers.count;
        uint32_t previousSectionIndex = kelf.sectionIndex;
        uint32_t previousSymbolIndex = kelf.symbolEntryIndex;
        void *previousHeader = kelf.header;
        unsigned int result = parseELF(file->read, file->context, length, &kelf);
        if (result != SYSTEM_CODE_SUCCESS)
            return (int)result;
        if (!kelf.isDone && previousStage == kelf.stage &&
            previousHeaderCount == kelf.headers.count &&
            previousSectionIndex == kelf.sectionIndex &&
            previousSymbolIndex == kelf.symbolEntryIndex &&
            previousHeader == kelf.header)
            return SYSTEM_CODE_INVALID_BINARY;
    }

    if (kelf.headers.count == 0U || vectorAt(&kelf.headers, 0U) == NULL)
        return SYSTEM_CODE_INVALID_BINARY;

    uint32_t loadedSegments = 0U;
    for (uint32_t i = 0; i < kelf.headers.count; ++i) {
        ElfProgramHeader *program = (ElfProgramHeader *)vectorAt(&kelf.headers, i);
        if (program == NULL)
            return SYSTEM_CODE_INVALID_BINARY;
        if (program->p_type != 1U)
            continue;
        if (program->p_filesz > program->p_memsz || program->p_offset > file->size ||
            program->p_filesz > file->size - program->p_offset)
            return SYSTEM_CODE_INVALID_BINARY;
        if (program->p_memsz == 0U)
            continue; /* empty LOAD segment, nothing to map */

        /* The loader uses identity paging in long mode, so the ELF virtual
         * address is also the address at which each PT_LOAD segment must live. */
        uint64_t loadAddress = kelf.bit == 2 ? program->p_vaddr :
            (program->p_paddr != 0U ? program->p_paddr : program->p_vaddr);
        if (loadAddress < 0x100000ULL || loadAddress >= 0x100000000ULL ||
            program->p_memsz > 0x100000000ULL - loadAddress)
            return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
        uint8_t *destination = (uint8_t *)fallocAt(loadAddress, (size_t)program->p_memsz);
        if (destination == NULL)
            return SYSTEM_CODE_ERROR_OUT_OF_MEMORY;

        uint64_t copied = 0U;
        uint8_t chunk[512];
        while (copied < program->p_filesz) {
            size_t chunkSize = (size_t)(program->p_filesz - copied);
            if (chunkSize > sizeof(chunk))
                chunkSize = sizeof(chunk);
            int result = file->read(file->context, program->p_offset + copied, chunk, chunkSize);
            if (result != SYSTEM_CODE_SUCCESS)
                return result;
            copyBytes(destination + copied, chunk, chunkSize);
            copied += chunkSize;
        }
        clearBytes(destination + program->p_filesz,
                   (size_t)(program->p_memsz - program->p_filesz));
        ++loadedSegments;
    }
    if (loadedSegments == 0U)
        return SYSTEM_CODE_INVALID_BINARY;

    const size_t stackSize = 2U * 1024U * 1024U;
    void *stack = falloc(stackSize);
    if (stack == NULL)
        return SYSTEM_CODE_ERROR_OUT_OF_MEMORY;

    image->entryPoint = kelf.bit == 1 ?
        ((Elf32_Ehdr *)kelf.header)->e_entry : ((Elf64_Ehdr *)kelf.header)->e_entry;
    if (kelf.bit == 2) {
        int entryIsExecutable = 0;
        for (uint32_t i = 0; i < kelf.headers.count; ++i) {
            ElfProgramHeader *program = (ElfProgramHeader *)vectorAt(&kelf.headers, i);
            if (program != NULL && program->p_type == 1U &&
                (program->p_flags & 1U) != 0U && image->entryPoint >= program->p_vaddr &&
                image->entryPoint - program->p_vaddr < program->p_memsz) {
                entryIsExecutable = 1;
                break;
            }
        }
        if (!entryIsExecutable || image->entryPoint >= 0x100000000ULL)
            return SYSTEM_CODE_INVALID_BINARY;
    }
    image->stackTop = (uintptr_t)stack + stackSize;
    image->stackSize = stackSize;
    image->elfClass = (uint32_t)kelf.bit;
    image->segmentCount = loadedSegments;
    return SYSTEM_CODE_SUCCESS;
}