/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header for MFT Implementation
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#include <ntifs.h>

/* NTFS file record numbers */
enum FileRecordNumbers
{
    MFT     = 0,
    MFTMirr = 1,
    LogFile = 2,
    Volume  = 3,
    AttrDef = 4,
    Root    = 5,
    Bitmap  = 6,
    Boot    = 7,
    BadClus = 8,
    Secure  = 9,
    UpCase  = 10,
    Extend  = 11,
};

/* Attribute types */
enum AttributeType
{
    StandardInformation = 0x10,
    AttributeList       = 0x20,
    FileName            = 0x30,
    ObjectId            = 0x40,
    SecurityDescriptor  = 0x50,
    VolumeName          = 0x60,
    VolumeInformation   = 0x70,
    Data                = 0x80,
    IndexRoot           = 0x90,
    IndexAllocation     = 0xA0,
    Bitmap              = 0xB0,
    ReparsePoint        = 0xC0,
    EAInformation       = 0xD0,
    EA                  = 0xE0,
    LoggedUtilityStream = 0x100,
};
#define ATTR_END 0xFFFFFFFF

/* File record flags */
#define FR_IN_USE        0x01
#define FR_IS_DIRECTORY  0x02
#define FR_IS_EXTENSION  0x04
#define FR_SPECIAL_INDEX 0x08

/* Attribute flags */
#define ATTR_COMPRESSED  0x0001
#define ATTR_ENCRYPTED   0x4000
#define ATTR_SPARSE      0x8000

struct FileRecordHeader
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

class MFT
{
    MFT::MFT();
    NTSTATUS MFT::GetFileRecord(ULONGLONG FileRecordNumber,
                                FileRecord* File);
};

class FileRecord
{
    FileRecordHeader Header;
    FileRecord::FileRecord();
    NTSTATUS FileRecord::FindUnnamedAttribute(ULONG Type, IAttribute* Attr);
    NTSTATUS FileRecord::FindNamedAttribute(ULONG Type,
                                            PCWSTR Name,
                                            ULONG NameLength,
                                            IAttribute* Attr);
};