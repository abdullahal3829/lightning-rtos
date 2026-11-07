/*
 * Part Public License (PPL)
 * Copyright (C) 2026 Abdullah Al Nahian/Raiyan
 * View LICENSE.md
 */

#ifndef TORITO_FS
#define TORITO_FS

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <protocol.h>

#define TORITO_SECTOR_SIZE 2048U

typedef struct __attribute__((packed)) {
    uint8_t  length;                    /* Offset  0: Length of Directory Record (LEN_DR) */
    uint8_t  ext_attr_rec_length;       /* Offset  1: Extended Attribute Record Length */
    uint8_t  location_of_extent[8];     /* Offset  2-9: Location of Extent (Both-endian 32-bit int) */
    uint8_t  data_length[8];            /* Offset 10-17: Data Length (Both-endian 32-bit int) */
    uint8_t  recording_date_time[7];    /* Offset 18-24: Recording Date and Time (7 bytes) */
    uint8_t  file_flags;                /* Offset 25: File Flags */
    uint8_t  file_unit_size;            /* Offset 26: File Unit Size */
    uint8_t  interleave_gap_size;       /* Offset 27: Interleave Gap Size */
    uint8_t  volume_sequence_number[4]; /* Offset 28-31: Volume Sequence Number (Both-endian 16-bit int) */
    uint8_t  file_identifier_len;       /* Offset 32: Length of File Identifier (LEN_FI) */
    uint8_t  file_identifier[1];        /* Offset 33+: File Identifier (Variable length buffer) */
} iso9660_directory_record_t;

typedef int (*ToritoDiskReadCallback)(void *context, uint64_t byteOffset,
                                      void *buffer, size_t size);

typedef struct {
    uint8_t drive;
    uint64_t extent;
    uint64_t size;
    uint64_t cachedSector;
    uint8_t cache[TORITO_SECTOR_SIZE];
    bool cacheValid;
} ToritoFile;

extern int isTorito(ToritoDiskReadCallback callback, void *context);
extern int nIsTorito(uint8_t drive);
extern int getToritoFileAddress(PartitionRegion region,
                                ToritoDiskReadCallback callback, void *context);
extern int toritoOpenFile(uint8_t drive, const char *path, ToritoFile *file);
extern int toritoReadFile(void *context, uint64_t offset, void *buffer, size_t size);

#endif // TORITO_FS