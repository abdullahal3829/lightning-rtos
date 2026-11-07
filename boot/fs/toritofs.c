/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <protocol.h>
#include "toritofs.h"
#include "../drivers/disk.h"
#include <definations.h>

#define BIOS_SECTOR_SIZE 512U
#define ISO_DESCRIPTOR_START 16u
#define ISO_DESCRIPTOR_LIMIT 128u
#define ISO_ROOT_RECORD_OFFSET 156U

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int readIsoBlock(ToritoDiskReadCallback callback, void *context,
                        uint64_t base, uint32_t lba, void *buffer) {
    return callback(context, base + (uint64_t)lba * TORITO_SECTOR_SIZE,
                    buffer, TORITO_SECTOR_SIZE);
}

static int readFromDrive(void *context, uint64_t byteOffset, void *buffer, size_t size) {
    uint8_t drive = *(const uint8_t *)context;
    if ((byteOffset % BIOS_SECTOR_SIZE) != 0U || (size % BIOS_SECTOR_SIZE) != 0U)
        return SYSTEM_CODE_ERROR_INVALID_SEEK;
    /*
     * When booting from CD-ROM, SeaBIOS' int 0x13 EDD handler interprets the
     * LBA in 2048-byte CD sectors (not 512-byte logical sectors) and transfers
     * a full 2048-byte CD sector per call.  Pass the CD-sector LBA so the
     * requested byte range maps to the correct CD sector.
     */
    return diskRead((char *)(uintptr_t)drive, byteOffset / TORITO_SECTOR_SIZE,
                    size, buffer);
}

static uint8_t upperAscii(uint8_t value) {
    if (value >= 'a' && value <= 'z')
        return (uint8_t)(value - ('a' - 'A'));
    return value;
}

static bool matchesFileName(const uint8_t *identifier, uint8_t identifierLength,
                            const char *path) {
    size_t pathLength = 0;
    while (path[pathLength] != '\0')
        ++pathLength;
    if (pathLength > 0U && path[0] == '/') {
        ++path;
        --pathLength;
    }

    size_t compareLength = identifierLength;
    for (size_t i = 0; i < identifierLength; ++i) {
        if (identifier[i] == ';') {
            compareLength = i;
            break;
        }
    }
    if (compareLength > 0U && identifier[compareLength - 1U] == '.')
        --compareLength;
    if (compareLength != pathLength)
        return false;
    for (size_t i = 0; i < compareLength; ++i) {
        if (upperAscii(identifier[i]) != upperAscii((uint8_t)path[i]))
            return false;
    }
    return true;
}

int isTorito(ToritoDiskReadCallback callback, void *context) {
    uint8_t descriptor[TORITO_SECTOR_SIZE];
    if (callback == NULL)
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
    for (uint32_t i = 0; i < ISO_DESCRIPTOR_LIMIT; ++i) {
        if (readIsoBlock(callback, context, 0U, ISO_DESCRIPTOR_START + i, descriptor) != SYSTEM_CODE_SUCCESS)
            return SYSTEM_CODE_ERROR_READ_FAULT;
        if (descriptor[1] != 'C' || descriptor[2] != 'D' || descriptor[3] != '0' ||
            descriptor[4] != '0' || descriptor[5] != '1' || descriptor[6] != 1U)
            return SYSTEM_CODE_ERROR_BAD_FORMAT;
        if (descriptor[0] == 255U)
            return false;
        if (descriptor[0] == 0U &&
            descriptor[7] == 'E' && descriptor[8] == 'L' && descriptor[9] == ' ' &&
            descriptor[10] == 'T' && descriptor[11] == 'O' && descriptor[12] == 'R' &&
            descriptor[13] == 'I' && descriptor[14] == 'T' && descriptor[15] == 'O' &&
            descriptor[16] == ' ' && descriptor[17] == 'S' && descriptor[18] == 'P' &&
            descriptor[19] == 'E' && descriptor[20] == 'C' && descriptor[21] == 'I' &&
            descriptor[22] == 'F' && descriptor[23] == 'I' && descriptor[24] == 'C' &&
            descriptor[25] == 'A' && descriptor[26] == 'T' && descriptor[27] == 'I' &&
            descriptor[28] == 'O' && descriptor[29] == 'N')
            return true;
    }
    return false;
}

int nIsTorito(uint8_t drive) {
    return isTorito(readFromDrive, &drive);
}

int getToritoFileAddress(PartitionRegion region, ToritoDiskReadCallback callback, void *context) {
    uint8_t descriptor[TORITO_SECTOR_SIZE];
    uint64_t base = region.base;
    uint64_t region_size = (uint64_t)region.size;
    uint32_t catalog_lba = 0;
    uint32_t i;
    if (callback == NULL)
        return -1;

    for (i = 0; i < ISO_DESCRIPTOR_LIMIT; ++i) {
        if (region_size && ((uint64_t)(ISO_DESCRIPTOR_START + i + 1) * TORITO_SECTOR_SIZE > region_size))
            return -1;
        if (readIsoBlock(callback, context, base, ISO_DESCRIPTOR_START + i, descriptor) != SYSTEM_CODE_SUCCESS)
            return -1;
        if (descriptor[1] != 'C' || descriptor[2] != 'D' || descriptor[3] != '0' ||
            descriptor[4] != '0' || descriptor[5] != '1' || descriptor[6] != 1)
            return -1;

        if (descriptor[0] == 255U)
            break;
        if (descriptor[0] == 0U &&
            descriptor[7] == 'E' && descriptor[8] == 'L' &&
            descriptor[9] == ' ' && descriptor[10] == 'T' &&
            descriptor[11] == 'O' && descriptor[12] == 'R' &&
            descriptor[13] == 'I' && descriptor[14] == 'T' &&
            descriptor[15] == 'O' && descriptor[16] == ' ' &&
            descriptor[17] == 'S' && descriptor[18] == 'P' &&
            descriptor[19] == 'E' && descriptor[20] == 'C' &&
            descriptor[21] == 'I' && descriptor[22] == 'F' &&
            descriptor[23] == 'I' && descriptor[24] == 'C' &&
            descriptor[25] == 'A' && descriptor[26] == 'T' &&
            descriptor[27] == 'I' && descriptor[28] == 'O' &&
            descriptor[29] == 'N') {
            catalog_lba = read_le32(&descriptor[71]);
            break;
        }
    }

    if (catalog_lba == 0)
        return -1;
    if (region_size && ((uint64_t)catalog_lba + 1) * TORITO_SECTOR_SIZE > region_size)
        return -1;
    if (callback(context, base + (uint64_t)catalog_lba * TORITO_SECTOR_SIZE,
                 descriptor, sizeof(descriptor)) != SYSTEM_CODE_SUCCESS)
        return -1;

    {
        uint32_t checksum = 0;
        for (i = 0; i < 32; i += 2)
            checksum += (uint32_t)descriptor[i] | ((uint32_t)descriptor[i + 1] << 8);
        if ((checksum & 0xffffu) != 0 || descriptor[30] != 0x55 || descriptor[31] != 0xaa)
            return -1;
    }
    if (descriptor[32] != 0x88)
        return -1;

    {
        uint32_t image_lba = read_le32(&descriptor[40]);
        if (region_size && (uint64_t)image_lba * TORITO_SECTOR_SIZE >= region_size)
            return -1;
        uint64_t imageOffset = base + (uint64_t)image_lba * TORITO_SECTOR_SIZE;
        return imageOffset > INT_MAX ? -1 : (int)imageOffset;
    }
}

int toritoOpenFile(uint8_t drive, const char *path, ToritoFile *file) {
    uint8_t block[TORITO_SECTOR_SIZE];
    uint8_t context = drive;
    if (path == NULL || file == NULL || path[0] == '\0')
        return SYSTEM_CODE_ERROR_INVALID_MEMORY_BLOCK;
    if (readIsoBlock(readFromDrive, &context, 0U, ISO_DESCRIPTOR_START, block) != SYSTEM_CODE_SUCCESS ||
        block[0] != 1U || block[1] != 'C' || block[2] != 'D' || block[3] != '0' ||
        block[4] != '0' || block[5] != '1' || block[6] != 1U)
        return SYSTEM_CODE_ERROR_BAD_FORMAT;
    if (read_le16(&block[128]) != TORITO_SECTOR_SIZE)
        return SYSTEM_CODE_ERROR_BAD_FORMAT;

    uint32_t volumeBlocks = read_le32(&block[80]);
    const uint8_t *rootRecord = &block[ISO_ROOT_RECORD_OFFSET];
    if (rootRecord[0] < 34U || (rootRecord[25] & 0x02U) == 0U)
        return SYSTEM_CODE_ERROR_BAD_FORMAT;
    uint32_t rootExtent = read_le32(&rootRecord[2]);
    uint32_t rootSize = read_le32(&rootRecord[10]);
    if (rootExtent >= volumeBlocks || rootSize == 0U ||
        (uint64_t)rootExtent + ((uint64_t)rootSize + TORITO_SECTOR_SIZE - 1U) /
            TORITO_SECTOR_SIZE > volumeBlocks)
        return SYSTEM_CODE_ERROR_BAD_FORMAT;

    uint64_t directoryOffset = 0U;
    while (directoryOffset < rootSize) {
        uint32_t blockIndex = (uint32_t)(directoryOffset / TORITO_SECTOR_SIZE);
        uint32_t blockOffset = (uint32_t)(directoryOffset % TORITO_SECTOR_SIZE);
        if (readIsoBlock(readFromDrive, &context, 0U, rootExtent + blockIndex, block) != SYSTEM_CODE_SUCCESS)
            return SYSTEM_CODE_ERROR_READ_FAULT;
        uint8_t recordLength = block[blockOffset];
        if (recordLength == 0U) {
            directoryOffset = ((uint64_t)blockIndex + 1U) * TORITO_SECTOR_SIZE;
            continue;
        }
        if (recordLength < 34U || blockOffset + recordLength > TORITO_SECTOR_SIZE ||
            directoryOffset + recordLength > rootSize)
            return SYSTEM_CODE_ERROR_BAD_FORMAT;
        uint8_t nameLength = block[blockOffset + 32U];
        if ((uint32_t)nameLength + 33U > recordLength)
            return SYSTEM_CODE_ERROR_BAD_FORMAT;
        if (nameLength > 0U && (block[blockOffset + 25U] & 0x82U) == 0U &&
            matchesFileName(&block[blockOffset + 33U], nameLength, path)) {
            uint32_t extent = read_le32(&block[blockOffset + 2U]);
            uint32_t size = read_le32(&block[blockOffset + 10U]);
            if ((uint64_t)extent + ((uint64_t)size + TORITO_SECTOR_SIZE - 1U) / TORITO_SECTOR_SIZE > volumeBlocks)
                return SYSTEM_CODE_ERROR_BAD_FORMAT;
            file->drive = drive;
            file->extent = extent;
            file->size = size;
            file->cachedSector = 0U;
            file->cacheValid = false;
            return SYSTEM_CODE_SUCCESS;
        }
        directoryOffset += recordLength;
    }
    return SYSTEM_CODE_ERROR_FILE_NOT_FOUND;
}

int toritoReadFile(void *context, uint64_t offset, void *buffer, size_t size) {
    ToritoFile *file = (ToritoFile *)context;
    uint8_t *destination = (uint8_t *)buffer;
    if (file == NULL || buffer == NULL || offset > file->size || size > file->size - offset)
        return SYSTEM_CODE_ERROR_INVALID_SEEK;

    uint64_t absoluteOffset = file->extent * (uint64_t)TORITO_SECTOR_SIZE + offset;
    size_t copied = 0U;
    while (copied < size) {
        uint64_t cdSector = absoluteOffset / TORITO_SECTOR_SIZE;
        size_t cdSectorOffset = (size_t)(absoluteOffset % TORITO_SECTOR_SIZE);
        size_t chunk = TORITO_SECTOR_SIZE - cdSectorOffset;
        if (chunk > size - copied)
            chunk = size - copied;
        if (!file->cacheValid || file->cachedSector != cdSector) {
            int result = diskRead((char *)(uintptr_t)file->drive, cdSector,
                                  TORITO_SECTOR_SIZE, file->cache);
            if (result != SYSTEM_CODE_SUCCESS)
                return result;
            file->cachedSector = cdSector;
            file->cacheValid = true;
        }
        for (size_t i = 0; i < chunk; ++i)
            destination[copied + i] = file->cache[cdSectorOffset + i];
        absoluteOffset += chunk;
        copied += chunk;
    }
    return SYSTEM_CODE_SUCCESS;
}