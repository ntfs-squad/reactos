/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

 /* *** Formerly: lfs/logfile.h *** */
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
 
 typedef struct NtfsLogRecord
 {
     UINT16 RedoOperation;              // Offset 0x00, Size 2
     UINT16 UndoOperation;              // Offset 0x02, Size 2
     UINT16 RedoOffset;                 // Offset 0x04, Size 2
     UINT16 RedoLength;                 // Offset 0x06, Size 2
     UINT16 UndoOffset;                 // Offset 0x08, Size 2
     UINT16 UndoLength;                 // Offset 0x0A, Size 2
     UINT16 TargetAttributeOffset;      // Offset 0x0C, Size 2
     UINT16 LCNsToFollow;               // Offset 0x0E, Size 2
     UINT16 RecordOffset;               // Offset 0x10, Size 2
     UINT16 AttributeOffset;            // Offset 0x12, Size 2
     UINT16 ClusterBlockOffset;         // Offset 0x14, Size 2
     UINT16 Reserved;                   // Offset 0x16, Size 2
     UINT64 TargetVCN;                  // Offset 0x18, Size 8
     UCHAR  LCNsForPage;                // Offset 0x20, Size Variable
 } *PNtfsLogRecord;
 
 typedef struct LfsClientRecord
 {
     UINT64 OldestLsn;                  // Offset 0x00, Size 8
     UINT64 ClientRestartLsn;           // Offset 0x08, Size 8
     UINT16 PrevClient;                 // Offset 0x10, Size 2
     UINT16 NextClient;                 // Offset 0x12, Size 2
     UINT16 SeqNumber;                  // Offset 0x14, Size 2
     UCHAR  Padding[6];                 // Offset 0x16, Size 6
     UINT32 ClientNameLength;           // Offset 0x1C, Size 4
     UCHAR  ClientName[128];            // Offset 0x20, Size 128
 } *PLfsClientRecord;
 
 typedef struct LfsRecordPage
 {
     WCHAR  Signature[4];               // Offset 0x00, Size 8
     UINT64 LastLsnOrFileOffset;        // Offset 0x08, Size 8
     UINT32 Flags;                      // Offset 0x10, Size 4
     UINT16 PageCount;                  // Offset 0x14, Size 2
     UINT16 PagePosition;               // Offset 0x16, Size 2
     UINT16 NextRecordOffset;           // Offset 0x18, Size 2
     UCHAR  Padding[6];                 // Offset 0x1A, Size 6
     UINT64 LastEndLsn;                 // Offset 0x20, Size 8
     USHORT UpdateSequenceArray;        // Offset 0x28, Size Variable
 } *PLfsRecordPage;
 
 typedef struct LfsRecord
 {
     UINT64 ThisLsn;                    // Offset 0x00, Size 8
     UINT64 ClientPreviousLsn;          // Offset 0x08, Size 8
     UINT64 ClientUndoNextLsn;          // Offset 0x10, Size 8
     UINT32 ClientDataLength;           // Offset 0x18, Size 4
     UINT16 ClientSeqNumber;            // Offset 0x1C, Size 2
     UINT16 ClientIndex;                // Offset 0x1E, Size 2
     UINT32 RecordType;                 // Offset 0x20, Size 4
     UINT32 TransactionId;              // Offset 0x24, Size 4
     UINT16 Flags;                      // Offset 0x28, Size 2
     UCHAR Padding[6];                  // Offset 0x2A, Size 6
     UCHAR Data;                        // Offset 0x30, Size (ClientDataLength)
 } *PLfsRecord;
 
 typedef struct LfsRestartPage
 {
     UCHAR  Signature[8];               // Offset 0x00, Size 8
     UINT64 ChkDskLsn;                  // Offset 0x08, Size 8
     UINT32 SystemPageSize;             // Offset 0x10, Size 4
     UINT32 LogPageSize;                // Offset 0x14, Size 4
     UINT16 RestartOffset;              // Offset 0x18, Size 2
     UINT16 MinorVersion;               // Offset 0x1A, Size 2
     UINT16 MajorVersion;               // Offset 0x1C, Size 2
     USHORT UpdateSequenceArray;        // Offset 0x1E, Size Variable
 } *PLfsRestartPage;
 
 typedef struct LfsRestartArea
 {
     UINT64 CurrentLsn;                 // Offset 0x00, Size 8
     UINT16 LogClients;                 // Offset 0x08, Size 2
     UINT16 ClientFreeList;             // Offset 0x0A, Size 2
     UINT16 ClientInUseList;            // Offset 0x0C, Size 2
     UINT16 Flags;                      // Offset 0x0E, Size 2
     UINT32 SeqNumberBits;              // Offset 0x10, Size 4
     UINT16 RestartAreaLength;          // Offset 0x14, Size 2
     UINT16 ClientArrayOffset;          // Offset 0x16, Size 2
     UINT64 FileSize;                   // Offset 0x18, Size 8
     UINT32 LastLsnDataLength;          // Offset 0x20, Size 4
     UINT16 RecordHeaderLength;         // Offset 0x24, Size 2
     UINT16 LogPageDataOffset;          // Offset 0x26, Size 2
     UINT32 RevisionNumber;             // Offset 0x28, Size 4
 } *PLfsRestartArea;
 
 /* Note: Client Version is stored in MajorVersion and MinorVersion
  * for the various versions of RestartArea/
  *
  * TODO: How do we determine which one to use?
  */
 
 // Used for disks formatted on Windows NT 4.0 (Size 64)
 typedef struct RestartArea
 {
     UINT32 MajorVersion;               // Offset 0x00, Size 4
     UINT32 MinorVersion;               // Offset 0x04, Size 4
     UINT64 StartOfCheckpointLsn;       // Offset 0x08, Size 8
     UINT64 OpenAttributeTableLsn;      // Offset 0x10, Size 8
     UINT64 AttributeNamesLsn;          // Offset 0x18, Size 8
     UINT64 DirtyPageTableLsn;          // Offset 0x20, Size 8
     UINT64 TransactionTableLsn;        // Offset 0x28, Size 8
     UINT32 OpenAttributeTableLength;   // Offset 0x30, Size 4
     UINT32 AttributeNamesLength;       // Offset 0x34, Size 4
     UINT32 DirtyPageTableLength;       // Offset 0x38, Size 4
     UINT32 TransactionTableLength;     // Offset 0x3C, Size 4
 } PRestartArea;
 
 // Used for disks formatted on Windows 2000 - Windows Server 2003 (Size 104)
 typedef struct RestartArea2
 {
     UINT32 MajorVersion;               // Offset 0x00, Size 4
     UINT32 MinorVersion;               // Offset 0x04, Size 4
     UINT64 StartOfCheckpointLsn;       // Offset 0x08, Size 8
     UINT64 OpenAttributeTableLsn;      // Offset 0x10, Size 8
     UINT64 AttributeNamesLsn;          // Offset 0x18, Size 8
     UINT64 DirtyPageTableLsn;          // Offset 0x20, Size 8
     UINT64 TransactionTableLsn;        // Offset 0x28, Size 8
     UINT32 OpenAttributeTableLength;   // Offset 0x30, Size 4
     UINT32 AttributeNamesLength;       // Offset 0x34, Size 4
     UINT32 DirtyPageTableLength;       // Offset 0x38, Size 4
     UINT32 TransactionTableLength;     // Offset 0x3C, Size 4
     UINT64 Unknown1;                   // Offset 0x40, Size 8
     UINT64 PreviousRestartRecordLsn;   // Offset 0x48, Size 8
     UINT32 BytesPerCluster;            // Offset 0x50, Size 4
     UCHAR  Padding[4];                 // Offset 0x54, Size 4
     UINT64 UsnJournal;                 // Offset 0x58, Size 8
     UINT64 Unknown2;                   // Offset 0x60, Size 8
 } *PRestartArea2;
 
 // Used for disks formatted on Windows Vista+ (Size 112)
 typedef struct RestartArea3
 {
     UINT32 MajorVersion;               // Offset 0x00, Size 4
     UINT32 MinorVersion;               // Offset 0x04, Size 4
     UINT64 StartOfCheckpointLsn;       // Offset 0x08, Size 8
     UINT64 OpenAttributeTableLsn;      // Offset 0x10, Size 8
     UINT64 AttributeNamesLsn;          // Offset 0x18, Size 8
     UINT64 DirtyPageTableLsn;          // Offset 0x20, Size 8
     UINT64 TransactionTableLsn;        // Offset 0x28, Size 8
     UINT32 OpenAttributeTableLength;   // Offset 0x30, Size 4
     UINT32 AttributeNamesLength;       // Offset 0x34, Size 4
     UINT32 DirtyPageTableLength;       // Offset 0x38, Size 4
     UINT32 TransactionTableLength;     // Offset 0x3C, Size 4
     UINT64 Unknown1;                   // Offset 0x40, Size 8
     UINT64 PreviousRestartRecordLsn;   // Offset 0x48, Size 8
     UINT32 BytesPerCluster;            // Offset 0x50, Size 4
     UCHAR  Padding[4];                 // Offset 0x54, Size 4
     UINT64 UsnJournal;                 // Offset 0x58, Size 8
     UINT64 Unknown2;                   // Offset 0x60, Size 8
     UINT64 UnknownLsn;                 // Offset 0x68, Size 8
 } *PRestartArea3;
 
 /* Note: When an NTFS disk is formatted from a PC
  * with 32-bit Windows, the client version is set to 0.0.
  * 64-bit Windows will set the client version to 1.0.
  */
 
 // Observed on NTFS 1.2, Client Version 0.0
 typedef struct OpenAttributeEntry
 {
     UINT32  AllocatedOrNextFree;       // Offset 0x00, Size 4
     UINT32  PointerToAttributeName;    // Offset 0x04, Size 4
     UINT64  FileReference;             // Offset 0x08, Size 8
     UINT64  LsnOfOpenRecord;           // Offset 0x10, Size 8
     BOOLEAN DirtyPagesSeen;            // Offset 0x18, Size 1
     BOOLEAN AttributeNamePresent;      // Offset 0x19, Size 1
     UCHAR   Padding[2];                // Offset 0x1A, Size 2
     UINT32  AttributeTypeCode;         // Offset 0x1C, Size 4
     UINT64  AttributeName;             // Offset 0x20, Size 8
     UINT32  BytesPerIndexBuffer;       // Offset 0x28, Size 4
 } *POpenAttributeEntry;
 
 // Observed on NTFS 3.0+, Client Version 0.0
 typedef struct OpenAttributeEntry2
 {
     UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
     UINT32 AttributeOffset;            // Offset 0x04, Size 4
     UINT64 FileReference;              // Offset 0x08, Size 8
     UINT64 LsnOfOpenRecord;            // Offset 0x10, Size 8
     UCHAR  Padding[4];                 // Offset 0x18, Size 4
     UINT32 AttributeTypeCode;          // Offset 0x1C, Size 4
     UINT64 PointerToAttributeName;     // Offset 0x20, Size 8
     UINT32 BytesPerIndexBuffer;        // Offset 0x28, Size 4
 } *POpenAttributeEntry2;
 
 // Observed on NTFS 3.0+, Client Version 1.0
 typedef struct OpenAttributeEntry3
 {
     UINT32  AllocatedOrNextFree;       // Offset 0x00, Size 4
     UINT32  BytesPerIndexBuffer;       // Offset 0x04, Size 4
     UINT32  AttributeTypeCode;         // Offset 0x08, Size 4
     BOOLEAN DirtyPagesSeen;            // Offset 0x0C, Size 1
     UCHAR   Padding[3];                // Offset 0x0D, Size 3
     UINT64  FileReference;             // Offset 0x10, Size 8
     UINT64  LsnOfOpenRecord;           // Offset 0x18, Size 8
     UINT64  PointerToAttributeName;    // Offset 0x20, Size 8
 } *POpenAttributeEntry3;
 
 // Used for Client Version 0.0
 typedef struct DirtyPageEntry
 {
     UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
     UINT32 TargetAttributeOffset;      // Offset 0x04, Size 4
     UINT32 LengthOfTransfer;           // Offset 0x08, Size 4
     UINT32 LCNsToFollow;               // Offset 0x0C, Size 4
     UINT32 Reserved;                   // Offset 0x10, Size 4
     UINT64 VCN;                        // Offset 0x14, Size 8
     UINT64 OldestLsn;                  // Offset 0x1C, Size 8
     UINT64 LCNsForPage;                // Offset 0x24, Size (8 * LCNsToFollow)
 } *PDirtyPageEntry;
 
 // Used for Client Version 1.0
 typedef struct DirtyPageEntry2
 {
     UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
     UINT32 TargetAttributeOffset;      // Offset 0x04, Size 4
     UINT32 LengthOfTransfer;           // Offset 0x08, Size 4
     UINT32 LCNsToFollow;               // Offset 0x0C, Size 4
     UINT64 VCN;                        // Offset 0x10, Size 8
     UINT64 OldestLsn;                  // Offset 0x18, Size 8
     UINT64 LCNsForPage;                // Offset 0x20, Size (8 * LCNsToFollow)
 } *PDirtyPageEntry2;
 
 typedef struct RestartTable
 {
     UINT16 EntrySize;                  // Offset 0x00, Size 2
     UINT16 NumberEntries;              // Offset 0x02, Size 2
     UINT16 NumberAllocated;            // Offset 0x04, Size 2
     UCHAR  Padding[6];                 // Offset 0x06, Size 6
     UINT32 FreeGoal;                   // Offset 0x0C, Size 4
     UINT32 FirstFree;                  // Offset 0x10, Size 4
     UINT32 LastFree;                   // Offset 0x14, Size 4
 } *PRestartTable;
 
 typedef struct TransactionEntry
 {
     UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
     UINT32 TransactionState;           // Offset 0x04, Size 4
     UINT64 FirstLsn;                   // Offset 0x08, Size 8
     UINT64 PreviousLsn;                // Offset 0x10, Size 8
     UINT64 UndoNextLsn;                // Offset 0x18, Size 8
     UINT32 UndoRecords;                // Offset 0x20, Size 4
     UINT32 UndoBytes;                  // Offset 0x24, Size 4
 } *PTransactionEntry;
 
 typedef struct AttributeNameEntry
 {
     UINT16 OpenAttributeOffset;        // Offset 0x00, Size 2
     UINT16 NameLength;                 // Offset 0x02, Size 2
     WCHAR  Name;                       // Offset 0x04, Size Variable
 } *PAttributeNameEntry;
 
typedef struct BitmapRange
{
    UINT32 BitmapOffset;               // Offset 0x00, Size 4
    UINT32 NumberOfBits;               // Offset 0x04, Size 4
} *PBitmapRange;

/* *** Formerly: lfs/usnjrnl.h *** */

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

/* *** Formerly: lfs/lfs.h *** */

typedef class RestartPage
{
    PLfsRestartPage Header;
    PLfsRestartArea Foo;

    // The head for an array of clients for the log file (usually just NTFS itself)
    PLfsClientRecord ClientArrayHead;
} *PRestartPage;

typedef class LogFileService
{
public:
    ULONG ClientMajorVersion;
    ULONG ClientMinorVersion;

    LogFileService(_In_ PNTFSVolume TargetVolume);
    ~LogFileService();
    NTSTATUS InitializeLFS();
    NTSTATUS LogTransaction();
    NTSTATUS CommitTransaction();
    NTSTATUS ShutdownLFS();
private:
    PNTFSVolume Volume;
    PFileRecord LogFile;
    PUCHAR LogFileData;

    PLfsRestartPage RestartPage1;
    PLfsRestartPage RestartPage2;

    // Call when creating LFS Object
    NTSTATUS PerformFileSystemRecovery();

    // Call every 5 seconds
    NTSTATUS WriteCheckpointRecord();

    BOOLEAN IsSupportedClientVersion()
    {
        // Supported versions currently include: 0.0, 1.0, 1.1
        if (ClientMajorVersion == 1)
        {
            return ClientMinorVersion == 0
                   || ClientMinorVersion == 1;
        }

        else if (ClientMajorVersion = 0)
        {
            return ClientMinorVersion == 0;
        }

        return FALSE;
    }
} *PLogFileService;
