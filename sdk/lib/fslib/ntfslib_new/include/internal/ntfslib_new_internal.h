
/* File record */

/* Private macros */
#define BytesPerCluster(Volume) (Volume->BytesPerSector * Volume->SectorsPerCluster)

#define FileRef(Key) ((Key)->Entry->Data.Directory.IndexedFile)

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

#define IsFileRecordInMFTMirr(FileRecordNumber) \
((DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector) > (FileRecordSize << 2)) ? \
((DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector) / FileRecordSize) < FileRecordNumber \
: FileRecordNumber < 4

#define LONGLONG_SIGN_EXTEND(Number, Bytes) \
(Number << ((sizeof(LONGLONG) - Bytes) * 8)) >> ((sizeof(LONGLONG) - Bytes) * 8)

#define BytesPerIndexRecord(DiskVolume) \
(BytesPerCluster(DiskVolume) * DiskVolume->ClustersPerIndexRecord)

// Used for LoadDirectory()
#define MayHaveShortKey(SearchKey) \
!(SearchKey->Flags & DIR_KEY_8DOT3) \
&& !(SearchKey->Entry->Flags & INDEX_ENTRY_END)

#define IsRootFile(Path) \
Path[0] == L'\0' || (Path[0] == L'\\' && Path[1] == L'\0')

#define MFTDiskOffset (MFTLCN * BytesPerCluster(DiskVolume))
#define MFTMirrDiskOffset (MFTMirrLCN * BytesPerCluster(DiskVolume))
#define FileRecordOffset(FileRecordNumber) (FileRecordNumber * FileRecordSize)

#define GetOffset(LCN) (LCN * BytesPerCluster(DiskVolume))
#define GetRunSize(Run) (Run->Length * BytesPerCluster(DiskVolume))

#define GetFileName(Key) \
((PFileNameEx)((Key)->Entry->IndexStream))

#define GetVCN(NodeKey) \
(PULONGLONG)(NodeKey->Entry + NodeKey->Entry->EntryLength - sizeof(ULONGLONG))

// Hack for now so I know what maps to what
#define GetIndexEntryVCN(IndexEntry) *GetVCN(IndexEntry)

// Calculates start of Index Buffer relative to the index allocation, given the node's VCN
#define GetAllocationOffsetFromVCN(VCN) \
(BytesPerIndexRecord(DiskVolume) < BytesPerCluster(DiskVolume)) ? \
(VCN * (DiskVolume->BytesPerSector)) : \
(VCN * BytesPerCluster(DiskVolume))

#define IsLastEntry(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_END)

#define IsIndexNode(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_NODE)

#define IsEndOfNode(Key) IsLastEntry(Key) && !IsIndexNode(Key)

#define GetNextKey(Key) \
IsIndexNode(Key) ? (Key)->ChildNode->FirstKey : IsEndOfNode(Key) ? \
(Key)->ParentNodeKey ? (Key)->ParentNodeKey->NextKey : NULL : (Key)->NextKey

#define IsLowercaseCharacterW(wchar) wchar >= L'a' && wchar <= L'z'

#define IsDotOrDotDotDirW(Buffer, Length) \
Buffer[0] == L'.' \
? Length == sizeof(WCHAR) || (Buffer[1] == L'.' && Length == 2 * sizeof(WCHAR)) \
: FALSE

#define DIR_KEY_8DOT3 1

#define GetWStrLength(x) ((x) * sizeof(WCHAR))
#define MAX_SHORTNAME_LENGTH 12
// #define ROUND_DOWN(N, S) ((N) - ((N) % (S)))
#define ULONG_ROUND_UP(x) ROUND_UP((x), (sizeof(ULONG)))

#define GetSubnodeVCN(Entry) (PULONGLONG)((ULONG_PTR)Entry + Entry->EntryLength - 8)

/* Private functions */
NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer);

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer);

// =========================
// NTFS Memory Tags
// =========================
// HACK: These should be private or in *km target.

// #ifndef TAG_NTFS
// #define TAG_NTFS 'NTFS'
// #endif
#ifndef TAG_MFT
#define TAG_MFT '$MFT'
#endif
#ifndef TAG_FILE_RECORD
#define TAG_FILE_RECORD 'FREC'
#endif
#ifndef TAG_DATA_RUN
#define TAG_DATA_RUN 'DTRN'
#endif
#ifndef TAG_LOG_FILE_SERVICE
#define TAG_LOG_FILE_SERVICE 'LgFS'
#endif
#ifndef TAG_BTREE
#define TAG_BTREE 'BTRE'
#endif

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

#ifdef __cplusplus

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

    LogFileService(_In_ PVolume TargetVolume);
    ~LogFileService();
    NTSTATUS InitializeLFS();
    NTSTATUS LogTransaction();
    NTSTATUS CommitTransaction();
    NTSTATUS ShutdownLFS();
private:
    PVolume DiskVolume;
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

#endif // __cplusplus

// TODO: Maybe we should remove this if NTFS_DEBUG isn't defined?
#include <debug.h>
#ifdef NTFS_DEBUG

/* Debug print functions. REMOVE WHEN DONE. */

static inline void PrintNTFSBootSector(PBootSector PartBootSector)
{
    DbgPrint("OEM ID            %s\n", PartBootSector->OEM_ID);
    DbgPrint("Bytes per sector  %ld\n", PartBootSector->BytesPerSector);
    DbgPrint("Sectors/cluster   %ld\n", PartBootSector->SectorsPerCluster);
    DbgPrint("Sectors per track %ld\n", PartBootSector->SectorsPerTrack);
    DbgPrint("Number of heads   %ld\n", PartBootSector->NumberOfHeads);
    DbgPrint("Sectors in volume %ld\n", PartBootSector->SectorsInVolume);
    DbgPrint("LCN for $MFT      %ld\n", PartBootSector->MFTLCN);
    DbgPrint("LCN for $MFT_MIRR %ld\n", PartBootSector->MFTMirrLCN);
    DbgPrint("Clusters/MFT Rec  %d\n", PartBootSector->ClustersPerFileRecord);
    DbgPrint("Clusters/IndexRec %d\n", PartBootSector->ClustersPerIndexRecord);
    DbgPrint("Serial number     0x%X\n", PartBootSector->SerialNumber);
};

#if 0
// Most of these broke with the driver migration.
#define PrintFlag(Item, Flag, FlagName) if(Item & Flag) \
DbgPrint("    %s\n", FlagName); \

static inline void PrintUpCaseTable(PUCHAR UpCaseData,
                                    ULONG Length)
{
    DbgPrint("Offset | Value\n");
    for (int i = 0; i < Length; i += 2)
    {
        DbgPrint("0x%2X   | %C\n", i, ((WCHAR)(UpCaseData[i])));
    }
}

static inline void PrintAttrDefTable(PNtfsFileRecord AttrDef)
{
    PAttrDefEntry TableEntry;
    ULONG AttrDefEntryIndex, AttrDefDataSize, MaxIndex;
    PUCHAR Buffer;
    PNtfsAttribute DataAttr;

    DataAttr = AttrDef->GetAttribute(TypeData, NULL);
    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDef->CopyData(DataAttr,
                      Buffer,
                      &AttrDefDataSize,
                      0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    AttrDefEntryIndex = 0;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;

    DbgPrint(" Type  | Name                       | Flags | Min  | Max  \n");
    DbgPrint("==========================================================\n");

    for (int i = 0; i < MaxIndex; i++)
    {
        DbgPrint(" 0x%03X | %-26S | 0x%02X  | 0x%02X | 0x%X\n",
                 TableEntry->AttributeType,
                 TableEntry->Label,
                 TableEntry->Flags,
                 TableEntry->MinimumSize,
                 TableEntry->MaximumSize);

        // Move onto the next element
        TableEntry++;
    }
    delete Buffer;
}

static inline void PrintFileRecordHeader(FileRecordHeader* FRH)
{
    DbgPrint("MFT Record Number: %ld\n", FRH->MFTRecordNumber);
}

static inline void PrintAttributeHeader(PAttribute Attr)
{
    DbgPrint("Attribute Type:        0x%X\n", Attr->AttributeType);
    DbgPrint("Length:                %ld\n", Attr->Length);
    DbgPrint("Nonresident Flag:      %ld\n", Attr->IsNonResident);
    DbgPrint("Name Length:           %ld\n", Attr->NameLength);
    DbgPrint("Name Offset:           %ld\n", Attr->NameOffset);
    DbgPrint("Flags:                 0x%X\n", Attr->Flags);
    DbgPrint("Attribute ID:          %ld\n", Attr->AttributeID);

    if (!(Attr->IsNonResident))
    {
        DbgPrint("Data Length:           %ld\n", Attr->Resident.DataLength);
        DbgPrint("Data Offset:           0x%X\n", Attr->Resident.DataLength);
        DbgPrint("Indexed Flag:          %ld\n", Attr->Resident.IndexedFlag);
    }

    else
    {
        DbgPrint("First VCN:             %ld\n", Attr->NonResident.FirstVCN);
        DbgPrint("Last VCN:              %ld\n", Attr->NonResident.LastVCN);
        DbgPrint("Data Run Offset:       %ld\n", Attr->NonResident.DataRunsOffset);
        DbgPrint("Compression Unit Size: %ld\n", Attr->NonResident.CompressionUnitSize);
        DbgPrint("Allocated Size:        %ld\n", Attr->NonResident.AllocatedSize);
        DbgPrint("Data Size:             %ld\n", Attr->NonResident.DataSize);
        DbgPrint("Initialized Data Size: %ld\n", Attr->NonResident.InitalizedDataSize);
    }
}

static inline void PrintFilenameAttrHeader(FileNameEx* Attr)
{
    UINT64 FRN = GetFRNFromFileRef(Attr->ParentFileReference);
    UINT16 SQN = GetSQNFromFileRef(Attr->ParentFileReference);

    DbgPrint("Parent Dir FRN:   %ld\n", FRN);
    DbgPrint("Parent Dir SQN:   %ld\n", SQN);
    DbgPrint("Creation Time:    %ld\n", Attr->CreationTime);
    DbgPrint("Last Write Time:  %ld\n", Attr->LastWriteTime);
    DbgPrint("Change Time:      %ld\n", Attr->ChangeTime);
    DbgPrint("Last Access Time: %ld\n", Attr->LastAccessTime);
    DbgPrint("Allocated Size:   %ld\n", Attr->AllocatedSize);
    DbgPrint("Data Size:        %ld\n", Attr->DataSize);
    DbgPrint("Flags:            0x%X\n", Attr->Flags);
    DbgPrint("Filename:        \"%S\"\n", Attr->Name);
}

static inline void PrintStdInfoEx(StandardInformationEx* StdInfo)
{
    DbgPrint("Change Time:      %lu\n", StdInfo->ChangeTime);
    DbgPrint("Last Access Time: %lu\n", StdInfo->LastAccessTime);
    DbgPrint("Last Write Time:  %lu\n", StdInfo->LastWriteTime);
    DbgPrint("Creation Time:    %lu\n", StdInfo->CreationTime);
}

static inline void PrintIndexRootEx(PIndexRootEx IndexRootData)
{
    DbgPrint("Attribute Type:            0x%X\n", IndexRootData->AttributeType);
    DbgPrint("Collation Rule:            0x%X\n", IndexRootData->CollationRule);
    DbgPrint("Bytes per Index Record:    %ld\n", IndexRootData->BytesPerIndexRec);
    DbgPrint("Clusters per Index Record: %ld\n", IndexRootData->ClusPerIndexRec);
}

static inline void PrintFileBothDirEntry(PFILE_BOTH_DIR_INFORMATION Data)
{
    DbgPrint("Short Name:        \"%S\"\n", Data->ShortName);
    DbgPrint("Short Name Length: %ld\n", Data->ShortNameLength);
    DbgPrint("File Name:         \"%S\"\n", Data->FileName);
    DbgPrint("File Name Length:  %ld\n", Data->FileNameLength);
}

static inline void PrintFileBothDirInfo(PFILE_BOTH_DIR_INFORMATION Info, UINT Depth)
{
    PFILE_BOTH_DIR_INFORMATION CurrentStruct = Info;

    for (int i = 0; i < Depth; i++)
    {
        if (CurrentStruct)
        {
            PrintFileBothDirEntry(CurrentStruct);
            if (CurrentStruct->NextEntryOffset)
                CurrentStruct = (PFILE_BOTH_DIR_INFORMATION)((char*)CurrentStruct + CurrentStruct->NextEntryOffset);
            else
                CurrentStruct = NULL;
        }
    }
}

static inline void PrintFileCreateOptions(UINT8 Disposition, ULONG CreateOptions)
{
    switch (Disposition)
    {
        case FILE_SUPERSEDE:
            DbgPrint("Disposition: FILE_SUPERSEDE\n");
            break;
        case FILE_CREATE:
            DbgPrint("Disposition: FILE_CREATE\n");
            break;
        case FILE_OPEN:
            DbgPrint("Disposition: FILE_OPEN\n");
            break;
        case FILE_OPEN_IF:
            DbgPrint("Disposition: FILE_OPEN_IF\n");
            break;
        case FILE_OVERWRITE:
            DbgPrint("Disposition: FILE_OVERWRITE\n");
            break;
        case FILE_OVERWRITE_IF:
            DbgPrint("Disposition: FILE_OVERWRITE_IF\n");
            break;
        default:
            DbgPrint("Disposition: UNKNOWN\n");
            break;
    }

    DbgPrint("Create Options Flags:\n");
    PrintFlag(CreateOptions, FILE_DIRECTORY_FILE, "FILE_DIRECTORY_FILE");
    PrintFlag(CreateOptions, FILE_NON_DIRECTORY_FILE, "FILE_NON_DIRECTORY_FILE");
    PrintFlag(CreateOptions, FILE_WRITE_THROUGH, "FILE_WRITE_THROUGH");
    PrintFlag(CreateOptions, FILE_SEQUENTIAL_ONLY, "FILE_SEQUENTIAL_ONLY");
    PrintFlag(CreateOptions, FILE_RANDOM_ACCESS, "FILE_RANDOM_ACCESS");
    PrintFlag(CreateOptions, FILE_NO_INTERMEDIATE_BUFFERING, "FILE_NO_INTERMEDIATE_BUFFERING");
    PrintFlag(CreateOptions, FILE_SYNCHRONOUS_IO_ALERT, "FILE_SYNCHRONOUS_IO_ALERT");
    PrintFlag(CreateOptions, FILE_SYNCHRONOUS_IO_NONALERT, "FILE_SYNCHRONOUS_IO_NONALERT");
    PrintFlag(CreateOptions, FILE_CREATE_TREE_CONNECTION, "FILE_CREATE_TREE_CONNECTION");
    PrintFlag(CreateOptions, FILE_COMPLETE_IF_OPLOCKED, "FILE_COMPLETE_IF_OPLOCKED");
    PrintFlag(CreateOptions, FILE_NO_EA_KNOWLEDGE, "FILE_NO_EA_KNOWLEDGE");
    PrintFlag(CreateOptions, FILE_OPEN_REPARSE_POINT, "FILE_OPEN_REPARSE_POINT");
    PrintFlag(CreateOptions, FILE_DELETE_ON_CLOSE, "FILE_DELETE_ON_CLOSE");
    PrintFlag(CreateOptions, FILE_OPEN_BY_FILE_ID, "FILE_OPEN_BY_FILE_ID");
    PrintFlag(CreateOptions, FILE_OPEN_FOR_BACKUP_INTENT, "FILE_OPEN_FOR_BACKUP_INTENT");
    // PrintFlag(CreateOptions, FILE_OPEN_REQUIRING_OPLOCK, "FILE_OPEN_REQUIRING_OPLOCK");
    PrintFlag(CreateOptions, FILE_RESERVE_OPFILTER, "FILE_RESERVE_OPFILTER");
}
#endif // 0 (hack comment out)

#endif // NTFS_DEBUG
