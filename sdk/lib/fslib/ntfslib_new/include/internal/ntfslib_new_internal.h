#include <ndk/iotypes.h>
#include <ndk/rtlfuncs.h>
#include <debug.h>

#include "ntfslib_new.h"
#include "ntfslib_new_internal_alloc.h"

/* Private macros */
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ULONG_ROUND_UP(x) ROUND_UP((x), (sizeof(ULONG)))

#define BytesPerCluster(Volume) (Volume->BytesPerSector * Volume->SectorsPerCluster)
#define FileRef(Key) ((Key)->Entry->Data.Directory.IndexedFile)
#define FileRecordIsDirectory(_File) \
    (!!(_File->Header->Flags & FR_IS_DIRECTORY))
#define IsFileRecordInMFTMirr(FileRecordNumber) \
    ((FileRecordNumber) < max(4u, BytesPerCluster(DiskVolume) / FileRecordSize))
#define LONGLONG_SIGN_EXTEND(Number, Bytes) \
    (Number << ((sizeof(LONGLONG) - Bytes) * 8)) >> ((sizeof(LONGLONG) - Bytes) * 8)
#define BytesPerIndexRecord(DiskVolume) \
    (BytesPerCluster(DiskVolume) * DiskVolume->ClustersPerIndexRecord)
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

#define IsLowercaseCharacterW(wchar) wchar >= L'a' && wchar <= L'z'

#define IsDotOrDotDotDirW(Buffer, Length) \
    Buffer[0] == L'.' \
    ? Length == sizeof(WCHAR) || (Buffer[1] == L'.' && Length == 2 * sizeof(WCHAR)) \
    : FALSE

#define DIR_KEY_8DOT3 1

#define GetWStrLength(x) ((x) * sizeof(WCHAR))
#define MAX_SHORTNAME_LENGTH 12
#define GetSubnodeVCN(Entry) (PULONGLONG)((ULONG_PTR)Entry + Entry->EntryLength - 8)

// Macro to get data pointer from a resident attribute pointer.
#define GetResidentDataPointer(Attrib) (char*)(((ULONG_PTR)Attrib) + \
                                               (Attrib->Resident.DataOffset))

// Macro to free memory from data run.
#define FreeDataRun(x) while(x) { \
    PDataRun tmp = x->NextRun;    \
    delete x;                     \
    x = tmp;                      \
}

// Macros to get values from a file reference
#define GetFRNFromFileRef(x) (x & 0xFFFFFFFFFFFF)
#define GetSQNFromFileRef(x) ((x << 48) >> 48) & 0xFFFF
#define GetNamePointer(x) (((char*)x) + (x->NameOffset))
#define GetAttributeDataSize(Attribute1) \
Attribute1->IsNonResident ? Attribute1->NonResident.DataSize : Attribute1->Resident.DataLength

#define NUMBER_OF_UPCASE_ENTRIES 0x10000

#ifdef __cplusplus
extern "C" {
#endif

VOID
NtfsAssertPagedCode(VOID);

ULONG
NtfsGetBytesPerSector(VOID);

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer);

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer);

#ifdef __cplusplus
}
#endif

// =========================
// NTFS Memory Tags
// =========================
// HACK: These should probably only be in *km target.

#ifndef TAG_NTFS
#define TAG_NTFS 'NTFS'
#endif
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

// =========================
// NTFS C++ Classes
// =========================

#ifdef __cplusplus

typedef class Volume
{
public:
    NtfsVersion Version;
    ULONG  BytesPerSector;
    UINT8  SectorsPerCluster;
    UINT32 ClustersInVolume;
    INT8   ClustersPerIndexRecord;
    UINT64 SerialNumber;
    ULONG Flags;
    ULONG OpenHandleCount;
    class MasterFileTable* MFT;
    class LogFileService* LFS;
    NtfsConfiguration Configuration;

    ~Volume();

    /**
     * Gets an attribute type value from the name of the attribute. This
     * performs a lookup against the $AttrDef metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                             _Out_ AttributeType* Type);

    /**
     * Gets the number of free clusters in the volume. This reads from the
     * $Bitmap metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters);

    /**
     * Converts a UNICODE_STRING to uppercase using the code page stored on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    Volume::UpcaseUnicodeString(_Inout_ PUNICODE_STRING String);

    /**
     * Converts a 16-bit string to uppercase using the code page stored on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    UpcaseWideString(_Inout_updates_(CharacterCount) PWSTR WideString,
                     _In_ ULONG CharacterCount);

    /**
     * Gets the volume label as a 16-bit string and its length in bytes.
     * This reads from the $Volume metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    GetVolumeLabel(_Inout_ PWSTR   VolumeLabel,
                   _Inout_ PUSHORT Length);

    /**
     * Sets the volume label from a 16-bit string and its length in bytes.
     * This writes to the $Volume metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    SetVolumeLabel(_In_ PWSTR VolumeLabel,
                   _In_ ULONG Length);

    /**
     * Copies a specified number of bytes into a buffer from the volume at a
     * given offset. The offset and length do not have to be sector aligned.
     *
     * @param Offset
     * The offset, in bytes, to begin copying data from the volume.
     *
     * @param Length
     * The number of bytes to copy from the volume.
     *
     * @param Buffer
     * The buffer to copy data from the volume into.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS ReadVolume(_In_    ULONGLONG Offset,
                        _In_    ULONG Length,
                        _Inout_ PUCHAR Buffer);

    /**
     * Writes a specified number of bytes to the volume at a given offset. The
     * offset and length do not have to be sector aligned.
     *
     * @param Offset
     * The offset, in bytes, to begin writing data to the volume.
     *
     * @param Length
     * The number of bytes to write to the volume.
     *
     * @param Buffer
     * The buffer containing the data to write to the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS WriteVolume(_In_    ULONGLONG Offset,
                         _In_    ULONG Length,
                         _Inout_ PUCHAR Buffer);

    NTSTATUS
    Initialize(_In_ const BootSector* PartitionBootSector,
               _In_opt_ PNtfsConfiguration Configuration);

    NTSTATUS
    GetADSPreference(_In_opt_ PWSTR FileNameQuery,
                     _Out_ AttributeType* RequestedType,
                     _Out_ PWSTR* RequestedStream);

    // ./sanity.cpp
    // These functions will likely be removed before the driver is released.
    void RunSanityChecks();
    void SanityCheckBlockIO();

private:
    NTSTATUS UpcaseStatus = STATUS_NOT_FOUND;
    NTSTATUS LoadUpcaseTable();
    PWCHAR UpcaseTable = NULL;

} *PVolume;

typedef class FileRecord
{
public:
    PFileRecordHeader Header;
    PUCHAR Data = NULL;

    // ./filerecord.cpp
    FileRecord(_In_ PVolume DiskVolume,
               _In_ ULONG FileRecordSize);
    FileRecord(_In_ PVolume DiskVolume);
    ~FileRecord();

    // ./find.cpp
    PAttribute GetAttribute(_In_     AttributeType Type,
                            _In_opt_ PWSTR Name);
    NTSTATUS GetAttributeData(_In_     AttributeType Type,
                              _In_opt_ PWSTR Name,
                              _Out_    PUCHAR *Data);
    PDataRun FindNonResidentData(_In_ PAttribute DataAttr);
    PDataRun FindNonResidentData(_In_     AttributeType Type,
                                 _In_opt_ PWSTR Name);

    // ./copy.cpp
    NTSTATUS CopyData(_In_ AttributeType Type,
                      _In_ PWSTR Name,
                      _In_ PUCHAR Buffer,
                      _Inout_ PULONG Length,
                      _In_ ULONGLONG Offset = 0);
    NTSTATUS CopyData(_In_ PAttribute Attr,
                      _In_ PUCHAR Buffer,
                      _Inout_ PULONG Length,
                      _In_ ULONGLONG Offset = 0);

    // ./write.cpp
    NTSTATUS
    WriteFileData(_In_     AttributeType AttrType,
                  _In_opt_ PWSTR StreamName,
                  _In_     PUCHAR Buffer,
                  _Inout_  PULONG Length,
                  _In_     PLARGE_INTEGER Offset);

    NTSTATUS
    UpdateResidentData(_In_ PAttribute TargetAttribute,
                       _In_ PUCHAR Buffer,
                       _In_ PULONG Length,
                       _In_ ULONGLONG Offset = 0);

    // ./ fixup.cpp
    NTSTATUS
    CommitFixup();

    NTSTATUS
    ApplyFixup();

private:
    PVolume DiskVolume;

    // ./write.cpp
    NTSTATUS
    UpdateNonResidentData(_In_ PAttribute TargetAttribute,
                          _In_ PUCHAR Buffer,
                          _In_ PULONG Length,
                          _In_ ULONGLONG Offset = 0);
} *PFileRecord;

#define INDEX_ENTRY_NODE 1
#define INDEX_ENTRY_END  2

struct BTreeKey;

typedef struct BTreeNode
{
    ULONGLONG VCN = 0;
    BTreeKey* FirstKey = NULL;

    ~BTreeNode();
} *PBTreeNode;

typedef struct BTreeKey
{
    /* In theory, a key does not have a link to parent node
     * In practice, we want to be able to jump back to the parent
     */
    BTreeKey* ParentNodeKey = NULL;
    PBTreeNode ChildNode = NULL;
    BTreeKey* NextKey = NULL;
    PIndexEntry Entry = NULL;
    ULONG Flags = 0;

    ~BTreeKey();

    bool IsLastEntry() const { return (Entry->Flags & INDEX_ENTRY_END) != 0; }
    bool IsEndOfNode() const { return IsLastEntry() && !ChildNode; }
    BTreeKey* NextInTree() const;
} *PBTreeKey;

typedef class Directory
{
public:
    // ./directory.cpp
    Directory(_In_ PVolume DiskVolume);
    NTSTATUS
    LoadDirectory(_In_ PFileRecord File);

    // ./find.cpp
    NTSTATUS
    FindNextFile(_In_  PWCHAR FileName,
                 _Out_ PULONGLONG FileRecordNumber);

    // ./get.cpp
    NTSTATUS
    GetFileBothDirInfo(_In_    BOOLEAN ReturnSingleEntry,
                       _In_    BOOLEAN RestartScan,
                       _In_    PUNICODE_STRING FileNameFilter,
                       _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                       _Inout_ PULONG BufferLength);

    // ./editdir.cpp
    NTSTATUS
    AddFileToDirectory(_In_ PFileNameEx FileToAdd);

    NTSTATUS
    RemoveFileFromDirectory(_In_ PBTreeKey FileToRemove);

    // ./dbg.cpp
    void
    DumpFileTree();

private:
    PVolume DiskVolume;
    BTreeNode RootNode;
    PBTreeKey CurrentKey = NULL;

    // ./directory.cpp
    NTSTATUS
    VerifyUpdateSequenceArray(PNTFSRecordHeader Record);
    NTSTATUS
    CreateNode(_In_    PFileRecord File,
               _In_    PAttribute  IndexAllocationAttribute,
               _Inout_ PBTreeKey   ParentNodeKey);
    NTSTATUS
    LoadRootNode(_In_ PFileRecord File,
                 _Inout_ PBTreeNode Node);
    NTSTATUS
    LoadNodeEntries(_In_ PFileRecord File,
                    _In_opt_ PAttribute IndexAllocation,
                    _In_ const IndexNodeHeader* Header,
                    _In_ ULONG HeaderBytes,
                    _Inout_ PBTreeNode Node,
                    _In_opt_ PBTreeKey ParentKey);
    BOOLEAN
    DoesFileNameMatch(PUNICODE_STRING NameFilter,
                      PBTreeKey Key,
                      BOOLEAN IgnoreCase = TRUE);
    PBTreeKey
    GetShortNameKey(_In_ PBTreeKey Key,
                    _In_ BOOLEAN SkipNonShortNames = TRUE);

    // ./find.cpp
    PBTreeKey
    FindKeyInNode(PUNICODE_STRING FileName,
                  PBTreeKey Key);

    // ./get.cpp
    BOOLEAN
    IsEligibleForFileDir(PBTreeKey Key,
                         PUNICODE_STRING FileNameFilter);

    // ./util.cpp
    BOOLEAN
    IsLegalShortNameCharacterW(_In_ WCHAR Char);
    BOOLEAN
    IsLegal8Dot3ShortName(_In_ PWSTR Buffer,
                          _In_ USHORT Length);
} *PDirectory;

typedef class MasterFileTable
{
public:
    UINT FileRecordSize;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    UINT64 MFTZoneEndLCN;

    // ./ mft.cpp
    MasterFileTable(_In_ PVolume TargetVolume,
                    _In_ UINT64  MFTLCN,
                    _In_ UINT64  MFTMirrLCN,
                    _In_ INT8    ClustersPerFileRecord);

    NTSTATUS
    WriteFileRecordToMFT(_In_ PFileRecord File);

    NTSTATUS
    IsFileRecordNumberInUse(_In_  ULONG FileRecordNumber,
                            _Out_ PBOOLEAN InUse);

    // ./get.cpp
    NTSTATUS
    GetFileRecord(_In_   ULONG FileRecordNumber,
                  _Out_  PFileRecord* File);

    NTSTATUS
    GetFileRecordFromMFTMirr(_In_  ULONG FileRecordNumber,
                             _Out_ PFileRecord* File);

    NTSTATUS
    GetFileRecordFromQuery(_In_  PWCHAR Query,
                           _Out_ PFileRecord* File);

    NTSTATUS
    GetFileAttributeFromFileRecordNumber(_In_  AttributeType Type,
                                         _In_  PWSTR Name,
                                         _In_  ULONG FileRecordNumber,
                                         _Out_ PFileRecord* TargetFile,
                                         _Out_ PAttribute* TargetAttribute);

private:
    ULONGLONG
    CalculateMftZoneEndLCN();

    PVolume DiskVolume;
    PFileRecord MFTFile = NULL;
    PFileRecord MFTMirrFile = NULL;
} *PMasterFileTable;
#endif // __cplusplus

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

typedef struct _NtfsLogRecord
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
} NtfsLogRecord, *PNtfsLogRecord;
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, RedoOperation,         0x00, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, UndoOperation,         0x02, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, RedoOffset,            0x04, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, RedoLength,            0x06, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, UndoOffset,            0x08, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, UndoLength,            0x0A, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, TargetAttributeOffset, 0x0C, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, LCNsToFollow,          0x0E, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, RecordOffset,          0x10, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, AttributeOffset,       0x12, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, ClusterBlockOffset,    0x14, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, Reserved,              0x16, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NtfsLogRecord, TargetVCN,             0x18, 8);
static_assert(offsetof(NtfsLogRecord, LCNsForPage)  == 0x20);

typedef struct _LfsClientRecord
{
    UINT64 OldestLsn;                  // Offset 0x00, Size 8
    UINT64 ClientRestartLsn;           // Offset 0x08, Size 8
    UINT16 PrevClient;                 // Offset 0x10, Size 2
    UINT16 NextClient;                 // Offset 0x12, Size 2
    UINT16 SeqNumber;                  // Offset 0x14, Size 2
    UCHAR  Padding[6];                 // Offset 0x16, Size 6
    UINT32 ClientNameLength;           // Offset 0x1C, Size 4
    UCHAR  ClientName[128];            // Offset 0x20, Size 128
} LfsClientRecord, *PLfsClientRecord;
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, OldestLsn,        0x00, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, ClientRestartLsn, 0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, PrevClient,       0x10, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, NextClient,       0x12, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, SeqNumber,        0x14, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, Padding,          0x16, 6);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, ClientNameLength, 0x1C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsClientRecord, ClientName,       0x20, 128);

typedef struct _LfsRecordPage
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
} LfsRecordPage, *PLfsRecordPage;
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, Signature,           0x00, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, LastLsnOrFileOffset, 0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, Flags,               0x10, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, PageCount,           0x14, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, PagePosition,        0x16, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, NextRecordOffset,    0x18, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, Padding,             0x1A, 6);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecordPage, LastEndLsn,          0x20, 8);
static_assert(offsetof(LfsRecordPage, UpdateSequenceArray) == 0x28);

typedef struct _LfsRecord
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
} LfsRecord, *PLfsRecord;
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, ThisLsn,           0x00, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, ClientPreviousLsn, 0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, ClientUndoNextLsn, 0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, ClientDataLength,  0x18, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, ClientSeqNumber,   0x1C, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, ClientIndex,       0x1E, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, RecordType,        0x20, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, TransactionId,     0x24, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, Flags,             0x28, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRecord, Padding,           0x2A, 6);
static_assert(offsetof(LfsRecord, Data) == 0x30);

typedef struct _LfsRestartPage
{
    UCHAR  Signature[8];               // Offset 0x00, Size 8
    UINT64 ChkDskLsn;                  // Offset 0x08, Size 8
    UINT32 SystemPageSize;             // Offset 0x10, Size 4
    UINT32 LogPageSize;                // Offset 0x14, Size 4
    UINT16 RestartOffset;              // Offset 0x18, Size 2
    UINT16 MinorVersion;               // Offset 0x1A, Size 2
    UINT16 MajorVersion;               // Offset 0x1C, Size 2
    USHORT UpdateSequenceArray;        // Offset 0x1E, Size Variable
} LfsRestartPage, *PLfsRestartPage;
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, Signature,      0x00, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, ChkDskLsn,      0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, SystemPageSize, 0x10, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, LogPageSize,    0x14, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, RestartOffset,  0x18, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, MinorVersion,   0x1A, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartPage, MajorVersion,   0x1C, 2);
static_assert(offsetof(LfsRestartPage, UpdateSequenceArray) == 0x1E);

typedef struct _LfsRestartArea
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
} LfsRestartArea, *PLfsRestartArea;
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, CurrentLsn,         0x00, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, LogClients,         0x08, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, ClientFreeList,     0x0A, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, ClientInUseList,    0x0C, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, Flags,              0x0E, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, SeqNumberBits,      0x10, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, RestartAreaLength,  0x14, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, ClientArrayOffset,  0x16, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, FileSize,           0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, LastLsnDataLength,  0x20, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, RecordHeaderLength, 0x24, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, LogPageDataOffset,  0x26, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(LfsRestartArea, RevisionNumber,     0x28, 4);

/* Note: Client Version is stored in MajorVersion and MinorVersion
 * for the various versions of RestartArea/
 *
 * TODO: How do we determine which one to use?
 */

// Used for disks formatted on Windows NT 4.0 (Size 64)
typedef struct _RestartArea
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
} RestartArea, *PRestartArea;
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, MajorVersion,             0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, MinorVersion,             0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, StartOfCheckpointLsn,     0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, OpenAttributeTableLsn,    0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, AttributeNamesLsn,        0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, DirtyPageTableLsn,        0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, TransactionTableLsn,      0x28, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, OpenAttributeTableLength, 0x30, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, AttributeNamesLength,     0x34, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, DirtyPageTableLength,     0x38, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea, TransactionTableLength,   0x3C, 4);

// Used for disks formatted on Windows 2000 - Windows Server 2003 (Size 104)
typedef struct _RestartArea2
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
} RestartArea2, *PRestartArea2;
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, MajorVersion,             0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, MinorVersion,             0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, StartOfCheckpointLsn,     0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, OpenAttributeTableLsn,    0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, AttributeNamesLsn,        0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, DirtyPageTableLsn,        0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, TransactionTableLsn,      0x28, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, OpenAttributeTableLength, 0x30, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, AttributeNamesLength,     0x34, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, DirtyPageTableLength,     0x38, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, TransactionTableLength,   0x3C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, Unknown1,                 0x40, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, PreviousRestartRecordLsn, 0x48, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, BytesPerCluster,          0x50, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, Padding,                  0x54, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, UsnJournal,               0x58, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea2, Unknown2,                 0x60, 8);

// Used for disks formatted on Windows Vista+ (Size 112)
typedef struct _RestartArea3
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
} RestartArea3, *PRestartArea3;
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, MajorVersion,             0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, MinorVersion,             0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, StartOfCheckpointLsn,     0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, OpenAttributeTableLsn,    0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, AttributeNamesLsn,        0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, DirtyPageTableLsn,        0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, TransactionTableLsn,      0x28, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, OpenAttributeTableLength, 0x30, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, AttributeNamesLength,     0x34, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, DirtyPageTableLength,     0x38, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, TransactionTableLength,   0x3C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, Unknown1,                 0x40, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, PreviousRestartRecordLsn, 0x48, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, BytesPerCluster,          0x50, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, Padding,                  0x54, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, UsnJournal,               0x58, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, Unknown2,                 0x60, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartArea3, UnknownLsn,               0x68, 8);

/* Note: When an NTFS disk is formatted from a PC
 * with 32-bit Windows, the client version is set to 0.0.
 * 64-bit Windows will set the client version to 1.0.
 */

// Observed on NTFS 1.2, Client Version 0.0
typedef struct _OpenAttributeEntry
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
} OpenAttributeEntry, *POpenAttributeEntry;
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, AllocatedOrNextFree,    0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, PointerToAttributeName, 0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, FileReference,          0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, LsnOfOpenRecord,        0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, DirtyPagesSeen,         0x18, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, AttributeNamePresent,   0x19, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, Padding,                0x1A, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, AttributeTypeCode,      0x1C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, AttributeName,          0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry, BytesPerIndexBuffer,    0x28, 4);

// Observed on NTFS 3.0+, Client Version 0.0
typedef struct _OpenAttributeEntry2
{
    UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
    UINT32 AttributeOffset;            // Offset 0x04, Size 4
    UINT64 FileReference;              // Offset 0x08, Size 8
    UINT64 LsnOfOpenRecord;            // Offset 0x10, Size 8
    UCHAR  Padding[4];                 // Offset 0x18, Size 4
    UINT32 AttributeTypeCode;          // Offset 0x1C, Size 4
    UINT64 PointerToAttributeName;     // Offset 0x20, Size 8
    UINT32 BytesPerIndexBuffer;        // Offset 0x28, Size 4
} OpenAttributeEntry2, *POpenAttributeEntry2;
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, AllocatedOrNextFree,    0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, AttributeOffset,        0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, FileReference,          0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, LsnOfOpenRecord,        0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, Padding,                0x18, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, AttributeTypeCode,      0x1C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, PointerToAttributeName, 0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry2, BytesPerIndexBuffer,    0x28, 4);

// Observed on NTFS 3.0+, Client Version 1.0
typedef struct _OpenAttributeEntry3
{
    UINT32  AllocatedOrNextFree;       // Offset 0x00, Size 4
    UINT32  BytesPerIndexBuffer;       // Offset 0x04, Size 4
    UINT32  AttributeTypeCode;         // Offset 0x08, Size 4
    BOOLEAN DirtyPagesSeen;            // Offset 0x0C, Size 1
    UCHAR   Padding[3];                // Offset 0x0D, Size 3
    UINT64  FileReference;             // Offset 0x10, Size 8
    UINT64  LsnOfOpenRecord;           // Offset 0x18, Size 8
    UINT64  PointerToAttributeName;    // Offset 0x20, Size 8
} OpenAttributeEntry3, *POpenAttributeEntry3;
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, AllocatedOrNextFree,    0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, BytesPerIndexBuffer,    0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, AttributeTypeCode,      0x08, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, DirtyPagesSeen,         0x0C, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, Padding,                0x0D, 3);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, FileReference,          0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, LsnOfOpenRecord,        0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(OpenAttributeEntry3, PointerToAttributeName, 0x20, 8);

// Used for Client Version 0.0
#pragma pack(push, 4)
typedef struct _DirtyPageEntry
{
    UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
    UINT32 TargetAttributeOffset;      // Offset 0x04, Size 4
    UINT32 LengthOfTransfer;           // Offset 0x08, Size 4
    UINT32 LCNsToFollow;               // Offset 0x0C, Size 4
    UINT32 Reserved;                   // Offset 0x10, Size 4
    UINT64 VCN;                        // Offset 0x14, Size 8
    UINT64 OldestLsn;                  // Offset 0x1C, Size 8
    UINT64 LCNsForPage;                // Offset 0x24, Size (8 * LCNsToFollow)
} DirtyPageEntry, *PDirtyPageEntry;
#pragma pack(pop)
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, AllocatedOrNextFree,   0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, TargetAttributeOffset, 0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, LengthOfTransfer,      0x08, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, LCNsToFollow,          0x0C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, Reserved,              0x10, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, VCN,                   0x14, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry, OldestLsn,             0x1C, 8);
static_assert(offsetof(DirtyPageEntry, LCNsForPage) == 0x24);

// Used for Client Version 1.0
typedef struct _DirtyPageEntry2
{
    UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
    UINT32 TargetAttributeOffset;      // Offset 0x04, Size 4
    UINT32 LengthOfTransfer;           // Offset 0x08, Size 4
    UINT32 LCNsToFollow;               // Offset 0x0C, Size 4
    UINT64 VCN;                        // Offset 0x10, Size 8
    UINT64 OldestLsn;                  // Offset 0x18, Size 8
    UINT64 LCNsForPage;                // Offset 0x20, Size (8 * LCNsToFollow)
} DirtyPageEntry2, *PDirtyPageEntry2;
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry2, AllocatedOrNextFree,   0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry2, TargetAttributeOffset, 0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry2, LengthOfTransfer,      0x08, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry2, LCNsToFollow,          0x0C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry2, VCN,                   0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(DirtyPageEntry2, OldestLsn,             0x18, 8);
static_assert(offsetof(DirtyPageEntry2, LCNsForPage) == 0x20);

typedef struct _RestartTable
{
    UINT16 EntrySize;                  // Offset 0x00, Size 2
    UINT16 NumberEntries;              // Offset 0x02, Size 2
    UINT16 NumberAllocated;            // Offset 0x04, Size 2
    UCHAR  Padding[6];                 // Offset 0x06, Size 6
    UINT32 FreeGoal;                   // Offset 0x0C, Size 4
    UINT32 FirstFree;                  // Offset 0x10, Size 4
    UINT32 LastFree;                   // Offset 0x14, Size 4
} RestartTable, *PRestartTable;
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, EntrySize,       0x00, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, NumberEntries,   0x02, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, NumberAllocated, 0x04, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, Padding,         0x06, 6);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, FreeGoal,        0x0C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, FirstFree,       0x10, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(RestartTable, LastFree,        0x14, 4);

typedef struct _TransactionEntry
{
    UINT32 AllocatedOrNextFree;        // Offset 0x00, Size 4
    UINT32 TransactionState;           // Offset 0x04, Size 4
    UINT64 FirstLsn;                   // Offset 0x08, Size 8
    UINT64 PreviousLsn;                // Offset 0x10, Size 8
    UINT64 UndoNextLsn;                // Offset 0x18, Size 8
    UINT32 UndoRecords;                // Offset 0x20, Size 4
    UINT32 UndoBytes;                  // Offset 0x24, Size 4
} TransactionEntry, *PTransactionEntry;
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, AllocatedOrNextFree, 0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, TransactionState,    0x04, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, FirstLsn,            0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, PreviousLsn,         0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, UndoNextLsn,         0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, UndoRecords,         0x20, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(TransactionEntry, UndoBytes,           0x24, 4);

typedef struct _AttributeNameEntry
{
    UINT16 OpenAttributeOffset;        // Offset 0x00, Size 2
    UINT16 NameLength;                 // Offset 0x02, Size 2
    WCHAR  Name;                       // Offset 0x04, Size Variable
} AttributeNameEntry, *PAttributeNameEntry;
ASSERT_FIELD_OFFSET_AND_LENGTH(AttributeNameEntry, OpenAttributeOffset, 0x00, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(AttributeNameEntry, NameLength,          0x02, 2);
static_assert(offsetof(AttributeNameEntry, Name) == 0x04);

typedef struct _BitmapRange
{
    UINT32 BitmapOffset;               // Offset 0x00, Size 4
    UINT32 NumberOfBits;               // Offset 0x04, Size 4
} BitmapRange, *PBitmapRange;
ASSERT_FIELD_OFFSET_AND_LENGTH(BitmapRange, BitmapOffset, 0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(BitmapRange, NumberOfBits, 0x04, 4);

/* *** Formerly: lfs/usnjrnl.h *** */

/* Notes:
 *  - UsnJrnl entries are aligned to 8-byte boundaries.
 */
typedef struct _UsnJrnlEntry
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
} UsnJrnlEntry, *PUsnJrnlEntry;
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, EntrySize,          0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, MajorVersion,       0x04, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, MinorVersion,       0x06, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, MFTReference,       0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, ParentMFTReference, 0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, EntryOffset,        0x18, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, Timestamp,          0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, Reason,             0x28, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, SourceInfo,         0x2C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, SecurityID,         0x30, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, FileAttributes,     0x34, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, FileNameSize,       0x38, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlEntry, FileNameOffset,     0x3A, 2);
static_assert(offsetof(UsnJrnlEntry, FileName) == 0x3C);

typedef struct _UsnJrnlMaxData
{
    UINT64 MaxSize;             // Offset 0x00, Size 8
    UINT64 AllocationDelta;     // Offset 0x08, Size 8
    UINT64 USN_ID;              // Offset 0x10, Size 8
    UINT64 LowestValidUSN;      // Offset 0x18, Size 8
} UsnJrnlMaxData, *PUsnJrnlMaxData;
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlMaxData, MaxSize,         0x00, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlMaxData, AllocationDelta, 0x08, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlMaxData, USN_ID,          0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(UsnJrnlMaxData, LowestValidUSN,  0x18, 8);

/* *** Formerly: lfs/lfs.h *** */

#ifdef __cplusplus

typedef struct _RestartPage
{
    PLfsRestartPage Header;
    PLfsRestartArea Foo;

    // The head for an array of clients for the log file (usually just NTFS itself)
    PLfsClientRecord ClientArrayHead;
} RestartPage, *PRestartPage;

// Supported versions currently include: 0.0, 1.0, 1.1
const NtfsLfsClientVersion SupportedLfsClientVersions[] = {{0, 0}, {1, 0}, {1, 1}};

typedef class LogFileService
{
public:
    NtfsLfsClientVersion ClientVersion;

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
        for (NtfsLfsClientVersion SupportedLfsClientVer : SupportedLfsClientVersions)
        {
            if (ClientVersion.MajorVersion == SupportedLfsClientVer.MajorVersion
                && ClientVersion.MinorVersion == SupportedLfsClientVer.MinorVersion)
                return TRUE;
        }

        return FALSE;
    }
} *PLogFileService;

#endif // __cplusplus

// TODO: Maybe we should remove this if NTFS_DEBUG isn't defined?
#include <debug.h>
#define NTFS_DEBUG
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
    Buffer = new(NtfsNonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
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
