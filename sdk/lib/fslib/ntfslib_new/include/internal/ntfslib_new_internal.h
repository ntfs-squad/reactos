#ifndef _NTFSLIB_NEW_INTERNAL_H_
#define _NTFSLIB_NEW_INTERNAL_H_

//Hack: Bad! Km only!
#include <ntifs.h>

/* Provides DPRINT1 for every library source (no-op in release builds).
 * Must stay unconditional: NTFS_DEBUG is never defined for the library
 * target itself, only by driver builds.
 */
#include <debug.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Environment contract: implemented by the linked env glue (km/um/fl). */
void*
NtfsAllocatePoolWithTag(_In_ POOL_TYPE PoolType,
                        _In_ size_t Size,
                        _In_ ULONG Tag);

void
NtfsFreePool(_In_ void* pObject);

BOOLEAN
NtfsIsNameInExpression(_In_     PUNICODE_STRING Expression,
                       _In_     PUNICODE_STRING Name,
                       _In_     BOOLEAN IgnoreCase,
                       _In_opt_ PWCHAR UpcaseTable);

/* Default copied into each volume when it is initialized. */
extern BOOLEAN NtfsDefaultShowMetadataFiles;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);
#endif

// =========================
// In-memory structures
// =========================

// Forward declarations for DataRun struct because it's a linked list.
struct DataRun;
typedef struct DataRun *PDataRun;

struct DataRun
{
    PDataRun  NextRun;
    ULONGLONG LCN;
    ULONGLONG Length; // In clusters
};

typedef struct AttrDefCacheEntry
{
    AttributeType Type;
    WCHAR Label[64];
} AttrDefCacheEntry, *PAttrDefCacheEntry;

struct _BTreeNode;
struct _BTreeKey;

typedef struct _BTreeNode BTreeNode, *PBTreeNode;
typedef struct _BTreeKey BTreeKey, *PBTreeKey;

#ifdef __cplusplus
void NtfsDestroyBTreeNode(_In_opt_ PBTreeNode Node);
#endif

NTSTATUS
NtfsApplyFixup(_Inout_ PNTFSRecordHeader Header,
               _In_ ULONG RecordSize,
               _In_ ULONG BytesPerSector);

NTSTATUS
NtfsCommitFixup(_Inout_ PNTFSRecordHeader Header,
                _In_ ULONG RecordSize,
                _In_ ULONG BytesPerSector);

struct _BTreeNode
{
    ULONGLONG  VCN;
    PBTreeKey  FirstKey;
};

struct _BTreeKey
{
    PBTreeKey   ParentNodeKey; // Used to get entries linearly.
    PBTreeNode  ChildNode;
    PBTreeKey   NextKey;
    PBTreeKey   ShortNameKey;
    PIndexEntry Entry;
    ULONG       Flags;
};

/* Private macros */
#define BytesPerCluster(Volume) (Volume->BytesPerSector * Volume->SectorsPerCluster)

#define FileRef(Key) ((Key)->Entry->Data.Directory.IndexedFile)

#define IsFileRecordInMFTMirr(FileRecordNumber) \
((DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector) > (FileRecordSize << 2)) ? \
FileRecordNumber < ((DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector) / FileRecordSize) \
: FileRecordNumber < 4

#define LONGLONG_SIGN_EXTEND(Number, Bytes) \
(((Number) << ((sizeof(LONGLONG) - (Bytes)) * 8)) >> ((sizeof(LONGLONG) - (Bytes)) * 8))

#define BytesPerIndexRecord(DiskVolume) \
((DiskVolume)->ClustersPerIndexRecord < 0 \
 ? (1UL << (-(DiskVolume)->ClustersPerIndexRecord)) \
 : (BytesPerCluster(DiskVolume) * (DiskVolume)->ClustersPerIndexRecord))

#define IsRootFile(Path) \
((Path)[0] == L'\0' || ((Path)[0] == L'\\' && (Path)[1] == L'\0'))

#define MFTDiskOffset (MFTLCN * BytesPerCluster(DiskVolume))
#define MFTMirrDiskOffset (MFTMirrLCN * BytesPerCluster(DiskVolume))
#define FileRecordOffset(FileRecordNumber) (FileRecordNumber * FileRecordSize)

#define GetOffset(LCN) (LCN * BytesPerCluster(DiskVolume))
#define GetRunSize(Run) (Run->Length * BytesPerCluster(DiskVolume))

#define GetFileName(Key) \
((PFileNameEx)((Key)->Entry->IndexStream))

// The VCN of an entry's subnode is stored in the last 8 bytes of the entry.
#define GetSubnodeVCN(Entry) \
((PULONGLONG)((ULONG_PTR)(Entry) + (Entry)->EntryLength - sizeof(ULONGLONG)))

// Calculates start of Index Buffer relative to the index allocation, given the node's VCN
#define GetAllocationOffsetFromVCN(VCN) \
(BytesPerIndexRecord(DiskVolume) < BytesPerCluster(DiskVolume)) ? \
(VCN * (DiskVolume->BytesPerSector)) : \
(VCN * BytesPerCluster(DiskVolume))

#define IsLastEntry(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_END)

#define IsIndexNode(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_NODE)

#define IsEndOfNode(Key) (IsLastEntry(Key) && !IsIndexNode(Key))

#ifdef __cplusplus
static inline PBTreeKey GetNextKey(PBTreeKey Key)
{
    if (IsIndexNode(Key))
        return Key->ChildNode->FirstKey;

    if (IsEndOfNode(Key))
        return Key->ParentNodeKey ? Key->ParentNodeKey->NextKey : NULL;

    return Key->NextKey;
}

static inline SIZE_T
NtfsWcsLen(_In_ PCWSTR String)
{
    PCWSTR End = String;
    while (*End)
        End++;
    return End - String;
}

static inline PWSTR
NtfsWcsChr(_In_ PWSTR String, _In_ WCHAR Character)
{
    do
    {
        if (*String == Character)
            return String;
    } while (*String++);
    return NULL;
}

static inline PCWSTR
NtfsWcsChr(_In_ PCWSTR String, _In_ WCHAR Character)
{
    do
    {
        if (*String == Character)
            return String;
    } while (*String++);
    return NULL;
}

static inline UNICODE_STRING
NtfsMakeCountedUnicodeString(_In_ PWSTR Buffer,
                             _In_ USHORT Length)
{
    UNICODE_STRING String;

    String.Buffer = Buffer;
    String.Length = Length;
    String.MaximumLength = Length;
    return String;
}
#endif

#define DIR_KEY_8DOT3 1

#define GetWStrLength(x) ((x) * sizeof(WCHAR))
#define MAX_SHORTNAME_LENGTH 12

// Macro to get data pointer from a resident attribute pointer.
#define GetResidentDataPointer(Attrib) (char*)(((ULONG_PTR)Attrib) + \
                                               (Attrib->Resident.DataOffset))

// Macro to free memory from data run.
#define FreeDataRun(x) while(x) {\
    PDataRun tmp = x->NextRun;\
    delete x;\
    x = tmp;\
}

// Macro to get the file record number from a file reference
#define GetFRNFromFileRef(x) ((x) & 0xFFFFFFFFFFFF)

#define GetNamePointer(x) (((char*)x) + (x->NameOffset))

#define GetAttributeDataSize(Attribute1) \
Attribute1->IsNonResident ? Attribute1->NonResident.DataSize : Attribute1->Resident.DataLength

#ifdef __cplusplus
extern "C" {
#endif

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
/* The core passes these into the allocation contract; each environment
 * decides whether to honor them (km pools do, the um heap ignores them).
 */

#define TAG_NTFS 'NTFS'
#define TAG_MFT '$MFT'
#define TAG_FILE_RECORD 'FREC'
#define TAG_DATA_RUN 'DTRN'
#define TAG_LOG_FILE_SERVICE 'LgFS'
#define TAG_BTREE 'BTRE'

// =========================
// NTFS C++ Classes
// =========================

#ifdef __cplusplus

typedef class Volume
{
public:
    ULONG  BytesPerSector;
    UINT8  SectorsPerCluster;
    UINT32 ClustersInVolume;
    INT8   ClustersPerIndexRecord;
    UINT64 SerialNumber;
    USHORT NtfsMajorVersion;
    USHORT NtfsMinorVersion;
    class MasterFileTable* MFT;
    class LogFileService* LFS;
    BOOLEAN IsReadOnly = FALSE;
    BOOLEAN ShowMetadataFiles = FALSE;

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
     * Converts a null-terminated 16-bit string to uppercase using the
     * code page stored on the volume. This reads from the $UpCase metadata
     * file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    UpcaseWideString(_Inout_ PWSTR WideString,
                     _In_    ULONG Length);

    PWSTR
    GetUpcaseTable()
    {
        return UpcaseTable;
    }

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
    Initialize(_In_ PUCHAR BootSectorData);

    NTSTATUS
    GetADSPreference(_In_  PUNICODE_STRING FileName,
                     _Out_ AttributeType* RequestedType,
                     _Out_ PWSTR* RequestedStream);

private:
    PAttrDefCacheEntry AttrDefCache = NULL;
    ULONG AttrDefCacheCount = 0;

    WCHAR VolumeLabelCache[MAXIMUM_VOLUME_LABEL_LENGTH / sizeof(WCHAR)] = {};
    USHORT VolumeLabelCacheLength = 0;
    BOOLEAN VolumeLabelCached = FALSE;

    /* $UpCase table, read from disk on first use by UpcaseWideString(). */
    PWSTR UpcaseTable = NULL;
    ULONG UpcaseTableLength = 0; // In WCHARs

    NTSTATUS
    LoadUpcaseTable();

    NTSTATUS
    LoadAttributeDefinitions();

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
    NTSTATUS ReadAttributeAlloc(_In_ PAttribute Attr,
                                _Outptr_result_bytebuffer_(*Length) PUCHAR* Buffer,
                                _Out_ PULONG Length);
    /* Same as above, but reuses an already-decoded data run list for
     * non-resident attributes instead of re-decoding it on every call.
     * The caller keeps ownership of the run list.
     */
    NTSTATUS CopyData(_In_ PAttribute Attr,
                      _In_opt_ PDataRun PrecomputedRuns,
                      _In_ PUCHAR Buffer,
                      _Inout_ PULONG Length,
                      _In_ ULONGLONG Offset);

    // ./write.cpp
    NTSTATUS
    WriteFileData(_In_     AttributeType AttrType,
                  _In_opt_ PWSTR StreamName,
                  _In_     PUCHAR Buffer,
                  _Inout_  PULONG Length,
                  _In_     PLARGE_INTEGER Offset,
                  _In_opt_ PDataRun PrecomputedRuns = NULL);

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
                          _In_ ULONGLONG Offset = 0,
                          _In_opt_ PDataRun PrecomputedRuns = NULL);
} *PFileRecord;

class BTree
{
public:
    NTSTATUS ResetCurrentKey();
protected:
    ~BTree();
    PBTreeNode RootNode;
    PBTreeKey CurrentKey;
};

typedef class Directory : BTree
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

    // ./directory.cpp
    NTSTATUS
    CreateNode(_In_    PFileRecord File,
                      _In_    PAttribute  IndexAllocationAttribute,
                      _In_    PDataRun    IndexAllocationRuns,
                      _In_    PAttribute  BitmapAttribute,
                      _Inout_ PBTreeKey   ParentNodeKey);
    NTSTATUS
    CreateRootNode(_In_  PFileRecord File,
                   _Out_ PBTreeNode *NewRootNode);
    BOOLEAN
    DoesFileNameMatch(PUNICODE_STRING NameFilter,
                      PBTreeKey Key,
                      BOOLEAN IgnoreCase = TRUE);
    PBTreeKey
    GetShortNameKey(_In_ PBTreeKey Key);

    // ./find.cpp
    PBTreeKey
    FindKeyInNode(PUNICODE_STRING FileName,
                  PBTreeKey Key);

    // ./get.cpp
    BOOLEAN
    IsEligibleForFileDir(PBTreeKey Key,
                         PUNICODE_STRING FileNameFilter);

} *PDirectory;

typedef class MasterFileTable
{
public:
    ULONG FileRecordSize;

    // ./ mft.cpp
    MasterFileTable(_In_ PVolume TargetVolume,
                    _In_ UINT64 MFTLCN,
                    _In_ UINT64 MFTMirrLCN,
                    _In_ INT8   ClustersPerFileRecord);
    ~MasterFileTable();

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
    PVolume DiskVolume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    PFileRecord MFTFile = NULL;
    PFileRecord MFTMirrFile = NULL;

    /* $DATA attribute and decoded run list of the cached $MFT/$MFTMirr
     * records, so every GetFileRecord() doesn't re-decode the runs.
     * Safe to cache while cluster (re)allocation is unimplemented.
     */
    PAttribute MFTDataAttr = NULL;
    PAttribute MFTMirrDataAttr = NULL;
    PDataRun MFTDataRuns = NULL;
    PDataRun MFTMirrDataRuns = NULL;
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
 } *PRestartArea;
 
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

typedef class LogFileService
{
public:
    ULONG ClientMajorVersion;
    ULONG ClientMinorVersion;

    LogFileService(_In_ PVolume TargetVolume);
    ~LogFileService();
    NTSTATUS InitializeLFS();
    NTSTATUS ShutdownLFS();

    /* Roadmap: LogTransaction()/CommitTransaction() for journaling and a
     * periodic WriteCheckpointRecord() will be added with the LFS write path.
     */
private:
    PVolume DiskVolume;
    PFileRecord LogFile;
    PUCHAR LogFileData;

    PLfsRestartPage RestartPage1;
    PLfsRestartPage RestartPage2;

    // Call when creating LFS Object
    NTSTATUS PerformFileSystemRecovery();

    BOOLEAN IsSupportedClientVersion()
    {
        // Supported versions currently include: 0.0, 1.0, 1.1
        if (ClientMajorVersion == 1)
        {
            return ClientMinorVersion == 0
                   || ClientMinorVersion == 1;
        }

        else if (ClientMajorVersion == 0)
        {
            return ClientMinorVersion == 0;
        }

        return FALSE;
    }
} *PLogFileService;

#endif // __cplusplus

#endif /* _NTFSLIB_NEW_INTERNAL_H_ */
