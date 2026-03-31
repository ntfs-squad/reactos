/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

/* Notes:
 *  - UsnJrnl entries are aligned to 8-byte boundaries.
 */
typedef struct UsnJrnlEntry
{
    UINT32 EntrySize;           // Offset 0x00, Size 4
    UINT16 MajorVersion;        // Offset 0x04, Size 2
    UINT16 MinorVersion;        // Offset 0x06, Size 2
    UINT64 MFTReference;        // Offset 0x08, Size 8
    UINT64 ParentMFTReference;  // Offset 0x10, Size 8
    UINT64 EntryOffset;         // Offset 0x18, Size 8
    UINT64 Timestamp;           // Offset 0x20, Size 8
    UINT32 Reason;              // Offset 0x28, Size 4
    UINT32 SourceInfo;          // Offset 0x2C, Size 4
    UINT32 SecurityID;          // Offset 0x30, Size 4
    UINT32 FileAttributes;      // Offset 0x34, Size 4
    UINT16 FileNameSize;        // Offset 0x38, Size 2
    UINT16 FileNameOffset;      // Offset 0x3A, Size 2
    UCHAR  FileName;            // Offset 0x3C, Size Variable
} *PUsnJrnlEntry;

typedef struct UsnJrnlMaxData
{
    UINT64 MaxSize;             // Offset 0x00, Size 8
    UINT64 AllocationDelta;     // Offset 0x08, Size 8
    UINT64 USN_ID;              // Offset 0x10, Size 8
    UINT64 LowestValidUSN;      // Offset 0x18, Size 8
} *PUsnJrnlMaxData;