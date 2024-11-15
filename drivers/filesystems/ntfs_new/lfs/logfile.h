/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

enum NtfsLogOperation
{
    NoOp                                 = 0x00,
    CompensationLogRecord                = 0x01,
    InitializeFileRecordSegment          = 0x02,
    DeallocateFileRecordSegment          = 0x03,
    WriteEndOfFileRecordSegment          = 0x04,
    CreateAttribute                      = 0x05,
    DeleteAttribute                      = 0x06,
    UpdateResidentAttributeValue         = 0x07,
    UpdateNonResidentAttributeValue      = 0x08,
    UpdateMappingPairs                   = 0x09,
    DeleteDirtyClusters                  = 0x0A,
    SetNewAttributeSizes                 = 0x0B,
    AddIndexEntryToRoot                  = 0x0C,
    DeleteIndexEntryFromRoot             = 0x0D,
    AddIndexEntryToAllocationBuffer      = 0x0E,
    DeleteIndexEntryFromAllocationBuffer = 0x0F,
    WriteEndOfIndexBuffer                = 0x10,
    SetIndexEntryVcnInRoot               = 0x11,
    SetIndexEntryVcnInAllocationBuffer   = 0x12,
    UpdateFileNameInRoot                 = 0x13,
    UpdateFileNameInAllocationBuffer     = 0x14,
    SetBitsInNonResidentBitMap           = 0x15,
    ClearBitsInNonResidentBitMap         = 0x16,
    HotFix                               = 0x17,
    EndTopLevelAction                    = 0x18,
    PrepareTransaction                   = 0x19,
    CommitTransaction                    = 0x1A,
    ForgetTransaction                    = 0x1B,
    OpenNonResidentAttribute             = 0x1C,
    OpenAttributeTableDump               = 0x1D,
    AttributeNamesDump                   = 0x1E,
    DirtyPageTableDump                   = 0x1F,
    TransactionTableDump                 = 0x20,
    UpdateRecordDataInRoot               = 0x21,
    UpdateRecordDataInAllocationBuffer   = 0x22
};

typedef struct LfsRestartPage
{
    WCHAR  Signature[4];        // Offset 0x00, Size 8
    UINT64 ChkDskLsn;           // Offset 0x08, Size 8
    UINT32 SystemPageSize;      // Offset 0x10, Size 4
    UINT32 LogPageSize;         // Offset 0x14, Size 4
    UINT16 RestartOffset;       // Offset 0x18, Size 2
    UINT16 MinorVersion;        // Offset 0x1A, Size 2
    UINT16 MajorVersion;        // Offset 0x1C, Size 2
    USHORT UpdateSequenceArray; // Offset 0x1E, Size Variable
} *PLfsRestartPage;

typedef struct LfsRestartArea
{
    UINT64 CurrentLsn;          // Offset 0x00, Size 8
    UINT16 LogClients;          // Offset 0x08, Size 2
    UINT16 ClientFreeList;      // Offset 0x0A, Size 2
    UINT16 ClientInUseList;     // Offset 0x0C, Size 2
    UINT16 Flags;               // Offset 0x0E, Size 2
    UINT32 SeqNumberBits;       // Offset 0x10, Size 4
    UINT16 RestartAreaLength;   // Offset 0x14, Size 2
    UINT16 ClientArrayOffset;   // Offset 0x16, Size 2
    UINT64 FileSize;            // Offset 0x18, Size 8
    UINT32 LastLsnDataLength;   // Offset 0x20, Size 4
    UINT16 RecordHeaderLength;  // Offset 0x24, Size 2
    UINT16 LogPageDataOffset;   // Offset 0x26, Size 2
    UINT32 RevisionNumber;      // Offset 0x28, Size 4
} *PLfsRestartArea;

typedef struct LfsClientRecord
{
    UINT64 OldestLsn;           // Offset 0x00, Size 8
    UINT64 ClientRestartLsn;    // Offset 0x08, Size 8
    UINT16 PrevClient;          // Offset 0x10, Size 2
    UINT16 NextClient;          // Offset 0x12, Size 2
    UINT16 SeqNumber;           // Offset 0x14, Size 2
    UCHAR  Padding[6];          // Offset 0x16, Size 6
    UINT32 ClientNameLength;    // Offset 0x1C, Size 4
    UCHAR  ClientName[128];     // Offset 0x20, Size 128
} *PLfsClientRecord;

typedef struct LfsRecordPage
{
    WCHAR  Signature[4];         // Offset 0x00, Size 8
    UINT64 LastLsnOrFileOffset;  // Offset 0x08, Size 8
    UINT32 Flags;                // Offset 0x10, Size 4
    UINT16 PageCount;            // Offset 0x14, Size 2
    UINT16 PagePosition;         // Offset 0x16, Size 2
    UINT16 NextRecordOffset;     // Offset 0x18, Size 2
    UCHAR  Padding[6];           // Offset 0x1A, Size 6
    UINT64 LastEndLsn;           // Offset 0x20, Size 8
    USHORT UpdateSequenceArray   // Offset 0x28, Size Variable
} *PLfsRecordPage;