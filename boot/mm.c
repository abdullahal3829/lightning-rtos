/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stddef.h>
#include <stdint.h>
#include <protocol.h>
#include <varutil.h>
#include "mm.h"

#if defined(EFI_BUILD) || defined(UEFI_BUILD)
#include <Uefi.h>

extern EFI_BOOT_SERVICES *gBS;
#endif

#if !defined(EFI_BUILD) && !defined(UEFI_BUILD)
/* -----------------------------------------------------------------------
 * BIOS path: E820 memory-map based allocation via MMAP_BASE table
 * --------------------------------------------------------------------- */

enum {
    MMAP_BASE = 0x0500,
    MMAP_END = 0x7C00,
    MMAP_ENTRY_SIZE = sizeof(CommonMemoryEntry),
};

extern volatile uint32_t mmap_count;
extern void PANIC(char *message);

CommonMemoryEntry getRegion(int index) {
    if (index < 0 || (uint32_t)index >= getRegionCount())
        return (CommonMemoryEntry){};

    const volatile CommonMemoryEntry *entries =
        (const volatile CommonMemoryEntry *)(uintptr_t)MMAP_BASE;

    static const char signature[] = "BTLDRREGION";
    for (uint32_t i = 0; i < sizeof(signature); ++i) {
        if (entries[index].signature[i] != signature[i]) {
            PANIC("Security Violation: Unexcepted Memory Region Is Corrurpted.");
            return (CommonMemoryEntry){};
        }
    }
    return entries[index];
}

uint32_t getRegionCount(void) {
    const uint32_t max_entries = (MMAP_END - MMAP_BASE) / MMAP_ENTRY_SIZE;
    return mmap_count < max_entries ? mmap_count : max_entries;
}

static void copyMemoryEntry(volatile CommonMemoryEntry *destination,
                            const volatile CommonMemoryEntry *source) {
    volatile uint8_t *dst = (volatile uint8_t *)destination;
    const volatile uint8_t *src = (const volatile uint8_t *)source;
    for (uint32_t i = 0; i < sizeof(CommonMemoryEntry); ++i)
        dst[i] = src[i];
}

static int reserveRegion(uint64_t base, uint64_t size) {
    const uint64_t address_limit = 0x100000000ULL;
    uint64_t end;
    if (size == 0U || base < 0x100000ULL || base >= address_limit ||
        size > address_limit - base)
        return 0;
    end = base + size;

    volatile CommonMemoryEntry *entries =
        (volatile CommonMemoryEntry *)(uintptr_t)MMAP_BASE;
    uint32_t count = getRegionCount();
    const uint32_t max_entries = (MMAP_END - MMAP_BASE) / MMAP_ENTRY_SIZE;

    for (uint32_t i = 0; i < count; ++i) {
        CommonMemoryEntry region = getRegion((int)i);
        uint64_t region_end;
        if (region.type != MREGION_USEABLE || region.region == 0U ||
            region.base >= address_limit || region.region > address_limit - region.base)
            continue;
        region_end = region.base + region.region;
        if (base < region.base || end > region_end)
            continue;

        if (base == region.base && end == region_end) {
            for (uint32_t j = i; j + 1U < count; ++j)
                copyMemoryEntry(&entries[j], &entries[j + 1U]);
            --mmap_count;
        } else if (base == region.base) {
            entries[i].base = end;
            entries[i].region = region_end - end;
        } else if (end == region_end)
            entries[i].region = base - region.base;
        else {
            if (count >= max_entries)
                return 0;
            for (uint32_t j = count; j > i + 1U; --j)
                copyMemoryEntry(&entries[j], &entries[j - 1U]);
            CommonMemoryEntry tail = region;
            tail.base = end;
            tail.region = region_end - end;
            copyMemoryEntry(&entries[i + 1U], (const volatile CommonMemoryEntry *)&tail);
            entries[i].region = base - region.base;
            ++mmap_count;
        }
        return 1;
    }
    return 0;
}
#endif /* !defined(EFI_BUILD) && !defined(UEFI_BUILD) */

/* -----------------------------------------------------------------------
 * Shared across BIOS and UEFI
 * --------------------------------------------------------------------- */

void* fallocAt(uint64_t base, size_t size) {
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS address = (EFI_PHYSICAL_ADDRESS)base;
    UINTN pages;

    if (size == 0U)
        return (void *)0;

    pages = (size + 0xFFFULL) / 0x1000ULL;
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &address);
    if (EFI_ERROR(status))
        return (void *)0;

    return (void *)(uintptr_t)address;
#else
    if (!reserveRegion(base, (uint64_t)size))
        return (void *)0;
    return (void *)(uintptr_t)base;
#endif
}

void* falloc(size_t size) {
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    EFI_STATUS status;
    VOID *buffer = NULL;

    if (size == 0U)
        return (void *)0;

    status = gBS->AllocatePool(EfiLoaderData, size, &buffer);
    if (EFI_ERROR(status))
        return (void *)0;

    return buffer;
#else
    const uint64_t address_limit = 0x100000000ULL;
    const uint64_t minimum_address = 0x100000ULL;
    uint64_t aligned_size;
    if (size == 0U || (uint64_t)size > address_limit - minimum_address - 0xFFFU)
        return (void *)0;
    aligned_size = ((uint64_t)size + 0xFFFU) & ~0xFFFULL;

    for (uint32_t i = 0; i < getRegionCount(); ++i) {
        CommonMemoryEntry region = getRegion((int)i);
        if (region.type != MREGION_USEABLE || region.region == 0U ||
            region.base >= address_limit || region.region > address_limit - region.base)
            continue;

        uint64_t base = region.base < minimum_address ? minimum_address : region.base;
        base = (base + 0xFFFU) & ~0xFFFULL;
        uint64_t region_end = region.base + region.region;
        if (base <= region_end && aligned_size <= region_end - base &&
            reserveRegion(base, aligned_size))
            return (void *)(uintptr_t)base;
    }
    return (void *)0;
#endif
}

uintptr_t allocateLoaderStackTop(void) {
    const size_t stackSize = 128U * 1024U;
    void *stack = falloc(stackSize);
    if (stack == NULL)
        return 0U;
    return (uintptr_t)stack + stackSize;
}

/* -----------------------------------------------------------------------
 * Vector helpers (shared, no firmware dependence)
 * --------------------------------------------------------------------- */

void vectorInit(vector *var) {
    if (var == NULL)
        return;
    var->base = NULL;
    var->sizePerBlocks = 0U;
    var->count = 0U;
    var->capacity = 0U;
    var->lockout = false;
}

int vectorAllocate(vector *var, unsigned int capacity, unsigned int blockSize) {
    if (var == NULL || capacity == 0U || blockSize == 0U ||
        capacity > (unsigned int)(SIZE_MAX / blockSize))
        return 0;

    void *base = falloc((size_t)capacity * blockSize);
    if (base == NULL)
        return 0;

    var->base = base;
    var->sizePerBlocks = blockSize;
    var->count = 0U;
    var->capacity = capacity;
    var->lockout = false;
    return 1;
}

int vectorAppend(vector *var, const void *base, unsigned int length) {
    if (var == NULL || var->base == NULL || base == NULL || var->lockout ||
        var->count >= var->capacity || length != var->sizePerBlocks)
        return 0;

    uint8_t *destination = (uint8_t *)var->base + (size_t)var->count * var->sizePerBlocks;
    const uint8_t *source = (const uint8_t *)base;
    for (unsigned int i = 0; i < length; ++i)
        destination[i] = source[i];
    ++var->count;
    return 1;
}

void *vectorAt(vector *var, unsigned int index) {
    if (var == NULL || var->base == NULL || index >= var->count)
        return NULL;
    return (uint8_t *)var->base + (size_t)index * var->sizePerBlocks;
}

void* frealloc(void* addr, size_t size) {
#if defined(EFI_BUILD) || defined(UEFI_BUILD)
    /* UEFI pool allocations are not individually resizable via AllocatePool.
     * Return NULL so callers fall back to allocate + copy + free. */
    (void)addr;
    (void)size;
    return (void *)0;
#else
    if (size == 0)
        goto freallocEnd;

    for (uint32_t i = 0; i < getRegionCount(); i++) {
        CommonMemoryEntry mentry = getRegion(i);
        if (mentry.base == (uint64_t)(uintptr_t)addr) {
            CommonMemoryEntry mnxtEntry = getRegion((int)i + 1);
            /*if ((mentry.region + mnxtEntry.region) >*/
            mentry.region = size;
            mnxtEntry.base -= size;
            uintptr_t baddr = MMAP_BASE;
            if ((baddr + (i * sizeof(CommonMemoryEntry)) > MMAP_END) ||
                (baddr + ((i + 1) * sizeof(CommonMemoryEntry)) > MMAP_END)) {
                PANIC("Security Violation: Buffer Overflown While Allocating Memory.");
                goto freallocEnd;
            }

            volatile CommonMemoryEntry *entries =
                (volatile CommonMemoryEntry *)(uintptr_t)baddr;
            if (i + 1 >= getRegionCount())
                goto freallocEnd;
            for (uint32_t j = 0; j < sizeof(CommonMemoryEntry); j++) {
                ((volatile uint8_t *)&entries[i])[j] =
                    ((const uint8_t *)&mentry)[j];
                ((volatile uint8_t *)&entries[i + 1])[j] =
                    ((const uint8_t *)&mnxtEntry)[j];
            }
            return (void*)(uintptr_t)mentry.base;
        }
    }
freallocEnd:
    return (void*)0;
#endif
}
