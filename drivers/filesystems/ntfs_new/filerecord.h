/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     File record header
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#include <ntifs.h>

/* File Record Flags */
#define FR_IN_USE        0x01
#define FR_IS_DIRECTORY  0x02
#define FR_IS_EXTENSION  0x04
#define FR_SPECIAL_INDEX 0x08
/* Attribute Flags */
#define ATTR_COMPRESSED  0x0001
#define ATTR_ENCRYPTED   0x4000
#define ATTR_SPARSE      0x8000

struct File_Record_Header
{
    ULONG     TypeID;                 // Offset 0x00, Size 4 (Should be 'FILE')
    USHORT    UpdateSequenceOffset;   // Offset 0x04, Size 2
    USHORT    SizeOfUpdateSequence;   // Offset 0x06, Size 2
    ULONGLONG LogFileSequenceNumber;  // Offset 0x08, Size 8
    USHORT    SequenceNumber;         // Offset 0x10, Size 2
    USHORT    HardLinkCount;          // Offset 0x12, Size 2
    USHORT    AttributeOffset;        // Offset 0x14, Size 2
    USHORT    Flags;                  // Offset 0x16, Size 2
    ULONG     ActualSize;             // Offset 0x18, Size 4
    ULONG     AllocatedSize;          // Offset 0x1C, Size 4
    ULONGLONG BaseFileRecord;         // Offset 0x20, Size 8
    USHORT    NextAttributeID;        // Offset 0x28, Size 2
    USHORT    Padding;
    ULONG     MFTRecordNumber;        // Offset 0x2C, Size 4
};

class IAttribute
{
    ULONG  AttributeType;             // Offset 0x00, Size 4
    ULONG  Length;                    // Offset 0x04, Size 4
    UCHAR  NonResidentFlag;           // Offset 0x08, Size 1
    UCHAR  NameLength;                // Offset 0x09, Size 1
    USHORT NameOffset;                // Offset 0x0A, Size 2
    USHORT Flags;                     // Offset 0x0C, Size 2
    USHORT AttributeID;               // Offset 0x0E, Size 2
};

class ResidentAttribute : IAttribute
{
    /* Value of AttributeOffset should be (2 * NameLength + 0x18).
     * Size of AttributeName should be (2 * NameLength).
     * Attribute contents has an offset of (2 * NameLength + 0x18).
     */
    ULONG  AttributeLength;           // Offset 0x10, Size 4
    USHORT AttributeOffset;           // Offset 0x14, Size 2
    UCHAR  IndexedFlag;               // Offset 0x16, Size 1
    UCHAR  AttributeName;             // Offset [NameOffset]
};

class NonResidentAttribute : IAttribute
{
    /* Value of DataRunsOffset should be (2 * NameLength + 0x40).
     * Size of AttributeName should be (2 * NameLength).
     * Data Run has an offset of (2 * NameLength + 0x40).
     */
    ULONGLONG FirstVCN;               // Offset 0x10, Size 8
    ULONGLONG LastVCN;                // Offset 0x18, Size 8
    USHORT    DataRunsOffset;         // Offset 0x20, Size 2
    USHORT    CompressionUnitSize;    // Offset 0x20, Size 2
    UCHAR     Reserved[4];
    ULONGLONG AllocatedAttributeSize; // Offset 0x28, Size 8
    ULONGLONG ActualAttributeSize;    // Offset 0x30, Size 8
    ULONGLONG InitalizedDataSize;     // Offset 0x38, Size 8
    UCHAR     AttributeName;          // Offset 0x40
};