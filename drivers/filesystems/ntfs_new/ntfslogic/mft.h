/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header for MFT Implementation
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */
#pragma once

#include <ntifs.h>
#include "ntfspartition.h"
#include "contextblocks.h"

/* NTFS file record numbers */
enum FileRecordNumbers
{
    _MFT     = 0,
    _MFTMirr = 1,
    _LogFile = 2,
    _Volume  = 3,
    _AttrDef = 4,
    _Root    = 5,
    _Bitmap  = 6,
    _Boot    = 7,
    _BadClus = 8,
    _Secure  = 9,
    _UpCase  = 10,
    _Extend  = 11,
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
    UINT32 TypeID;                 // Offset 0x00, Size 4 (Should be 'FILE')
    UINT16 UpdateSequenceOffset;   // Offset 0x04, Size 2
    UINT16 SizeOfUpdateSequence;   // Offset 0x06, Size 2
    UINT64 LogFileSequenceNumber;  // Offset 0x08, Size 8
    UINT16 SequenceNumber;         // Offset 0x10, Size 2
    UINT16 HardLinkCount;          // Offset 0x12, Size 2
    UINT16 AttributeOffset;        // Offset 0x14, Size 2
    UINT16 Flags;                  // Offset 0x16, Size 2
    UINT32 ActualSize;             // Offset 0x18, Size 4
    UINT32 AllocatedSize;          // Offset 0x1C, Size 4
    UINT64 BaseFileRecord;         // Offset 0x20, Size 8
    UINT16 NextAttributeID;        // Offset 0x28, Size 2
    UINT16 Padding;
    UINT32 MFTRecordNumber;        // Offset 0x2C, Size 4
};

class IAttribute
{
public:
    UINT32 AttributeType;             // Offset 0x00, Size 4
    UINT32 Length;                    // Offset 0x04, Size 4
    UINT8  NonResidentFlag;           // Offset 0x08, Size 1
    UINT8  NameLength;                // Offset 0x09, Size 1
    UINT16 NameOffset;                // Offset 0x0A, Size 2
    UINT16 Flags;                     // Offset 0x0C, Size 2
    UINT16 AttributeID;               // Offset 0x0E, Size 2
};

class ResidentAttribute : public IAttribute
{
    /* Value of AttributeOffset should be (2 * NameLength + 0x18).
     * Size of AttributeName should be (2 * NameLength).
     * Attribute contents has an offset of (2 * NameLength + 0x18).
     */
public:
    UINT32 AttributeLength;           // Offset 0x10, Size 4
    UINT16 AttributeOffset;           // Offset 0x14, Size 2
    UINT8  IndexedFlag;               // Offset 0x16, Size 1
};

class NonResidentAttribute : public IAttribute
{
    /* Value of DataRunsOffset should be (2 * NameLength + 0x40).
     * Size of AttributeName should be (2 * NameLength).
     * Data Run has an offset of (2 * NameLength + 0x40).
     */
public:
    UINT64 FirstVCN;               // Offset 0x10, Size 8
    UINT64 LastVCN;                // Offset 0x18, Size 8
    UINT16 DataRunsOffset;         // Offset 0x20, Size 2
    UINT16 CompressionUnitSize;    // Offset 0x20, Size 2
    UINT32 Reserved;               // Offset 0x22, Size 4
    UINT64 AllocatedAttributeSize; // Offset 0x28, Size 8
    UINT64 ActualAttributeSize;    // Offset 0x30, Size 8
    UINT64 InitalizedDataSize;     // Offset 0x38, Size 8
};

class FileRecord
{
public:
    NTSTATUS FileRecord::LoadData(PUCHAR FileRecordData, unsigned Length);
    NTSTATUS FileRecord::FindUnnamedAttribute(ULONG Type,
                                              IAttribute* Attr,
                                              PUCHAR Data);
    NTSTATUS FileRecord::FindNamedAttribute(ULONG Type,
                                            PCWSTR Name,
                                            IAttribute* Attr,
                                            PUCHAR Data);
private:
    FileRecordHeader *Header;
    UCHAR AttrData[0x1000]; //TODO:
    ULONG AttrLength;
};

class MFT
{
public:
    MFT::MFT(VolumeContextBlock* VCB, PDEVICE_OBJECT DeviceObj);
    NTSTATUS MFT::GetFileRecord(ULONGLONG FileRecordNumber, FileRecord* File);
private:
    NtfsPartition* CurrentPartition;
    VolumeContextBlock* VCB;
    PDEVICE_OBJECT PartDeviceObj;
};
