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
#include "io/contextblocks.h"
#include "attributes.h"

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

/* File record flags */
#define FR_IN_USE        0x01
#define FR_IS_DIRECTORY  0x02
#define FR_IS_EXTENSION  0x04
#define FR_SPECIAL_INDEX 0x08

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

class FileRecord
{
public:
    NTSTATUS LoadData(PUCHAR FileRecordData, unsigned Length);
    NTSTATUS FindAttribute(_In_ AttributeType Type,
                           _Out_ PCWSTR Name,
                           _Out_ IAttribute* Attr,
                           _Out_ PUCHAR Data);
    NTSTATUS FindFileNameAttribute(_Out_ ResidentAttribute* Attr,
                                   _Out_ FileNameEx* AttrHeaderEx,
                                   _Out_ PWSTR Filename);
    NTSTATUS FindVolumeNameAttribute(_Out_ ResidentAttribute* Attr,
                                     _Out_ PWSTR Data);
    NTSTATUS FindStandardInformationAttribute(_Out_ ResidentAttribute* Attr,
                                              _Out_ StandardInformationEx* AttrHeaderEx);
private:
    FileRecordHeader *Header;
    UCHAR AttrData[0x1000]; //TODO: Figure out proper size
    ULONG AttrLength;
};

class MFT
{
public:
    MFT::MFT(_In_ PNtfsPartition ParentPartition);
    NTSTATUS MFT::GetFileRecord(ULONGLONG FileRecordNumber, FileRecord* File);
private:
    NtfsPartition* NtfsPart;
};
