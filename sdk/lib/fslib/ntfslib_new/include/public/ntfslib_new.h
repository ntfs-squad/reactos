// Hack: This whole header is a hack replacement for ntfspch.h

// Hack: This should only be in enviornments/km.cpp
#include <ntifs.h>

#ifdef __cplusplus
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);
extern "C" {
    // Hack: This is a driver-specific setting. Our lib should not care.
    extern BOOLEAN gShowMetadataFiles;
}
#endif

#ifndef UINT
typedef unsigned int UINT;
#endif

// Hack: This should be private or in *km target.
#ifndef TAG_NTFS
#define TAG_NTFS 'NTFS'
#endif

// Attribute end marker
#define ATTR_END           0xFFFFFFFF

// Attribute flags
#define ATTR_COMPRESSED    0x0001
#define ATTR_ENCRYPTED     0x4000
#define ATTR_SPARSE        0x8000

// FileName flags
#define FN_READONLY        0x0001
#define FN_HIDDEN          0x0002
#define FN_SYSTEM          0x0004
#define FN_ARCHIVE         0x0020
#define FN_DEVICE          0x0040
#define FN_TEMP            0x0100
#define FN_SPARSE          0x0200
#define FN_REPARSE         0x0400
#define FN_COMPRESSED      0x0800
#define FN_OFFLINE         0x1000
#define FN_NOTINDEXED      0x2000
#define FN_ENCRYPTED       0x4000
#define FN_DIRECTORY       0x10000000
#define FN_INDEX_VIEW      0x20000000

// FileName types
#define NAME_TYPE_POSIX 0             // 0b00
#define NAME_TYPE_WIN32 1             // 0b01
#define NAME_TYPE_DOS   2             // 0b10
#define NAME_TYPE_WIN32_AND_DOS 3     // 0b11

// Volume Information Flags
#define VOL_DIRTY          0x0001
#define VOL_RESIZE_LOG     0x0002
#define VOL_UPGRADE_ON_MNT 0x0004
#define VOL_USN_DEL_INPROG 0x0010
#define VOL_REPAIR_OBJ_IDS 0x0020
#define VOL_CHECKED        0x8000

/* Attribute types
 * Note that TypeVolumeVersion and TypeSymbolicLink existed in Windows NT 4 and
 * older but was unused. They were superseded by TypeObjectId and
 * TypeReparsePoint. TypePropertySet existed in Windows 2000 beta builds for
 * Native Structured Storage (NSS) before being canceled for Beta 3.
 */
enum AttributeType
{
    TypeStandardInformation = 0x10,
    TypeAttributeList       = 0x20,
    TypeFileName            = 0x30,
    TypeObjectId            = 0x40,
    TypeVolumeVersion       = 0x40,
    TypeSecurityDescriptor  = 0x50,
    TypeVolumeName          = 0x60,
    TypeVolumeInformation   = 0x70,
    TypeData                = 0x80,
    TypeIndexRoot           = 0x90,
    TypeIndexAllocation     = 0xA0,
    TypeBitmap              = 0xB0,
    TypeReparsePoint        = 0xC0,
    TypeSymbolicLink        = 0xC0,
    TypeEAInformation       = 0xD0,
    TypeEA                  = 0xE0,
    TypePropertySet         = 0xF0,
    TypeLoggedUtilityStream = 0x100,
    TypeAttributeEndMarker  = 0xFFFFFFFF
};

#ifndef __cplusplus
typedef enum AttributeType AttributeType;
#endif

typedef struct
{
    UINT32 AttributeType;                  // Offset 0x00, Size 4
    UINT32 Length;                         // Offset 0x04, Size 4
    UINT8  IsNonResident;                  // Offset 0x08, Size 1
    UINT8  NameLength;                     // Offset 0x09, Size 1
    UINT16 NameOffset;                     // Offset 0x0A, Size 2
    UINT16 Flags;                          // Offset 0x0C, Size 2
    UINT16 AttributeID;                    // Offset 0x0E, Size 2
    union
    {
        struct
        {
            /* Value of AttributeOffset should be (2 * NameLength + 0x18).
             * Size of AttributeName should be (2 * NameLength).
             * Attribute contents has an offset of (2 * NameLength + 0x18).
             */
            UINT32 DataLength;             // Offset 0x10, Size 4
            UINT16 DataOffset;             // Offset 0x14, Size 2
            UINT8  IndexedFlag;            // Offset 0x16, Size 1
            UINT8  Padding;                // Offset 0x17, Size 1
        } Resident;
        struct
        {
            /* Value of DataRunsOffset should be (2 * NameLength + 0x40).
            * Size of AttributeName should be (2 * NameLength).
            * Data Run has an offset of (2 * NameLength + 0x40).
            */
            UINT64 FirstVCN;               // Offset 0x10, Size 8
            UINT64 LastVCN;                // Offset 0x18, Size 8
            UINT16 DataRunsOffset;         // Offset 0x20, Size 2
            UINT16 CompressionUnitSize;    // Offset 0x20, Size 2
            UINT32 Reserved;               // Offset 0x22, Size 4
            UINT64 AllocatedSize;          // Offset 0x28, Size 8
            UINT64 DataSize;               // Offset 0x30, Size 8
            UINT64 InitalizedDataSize;     // Offset 0x38, Size 8
            // This was in the old driver but I think this is wrong.
            // UINT64 CompressedDataSize;     // Offset 0x40, Size 8
        } NonResident;
    };
} Attribute, *PAttribute;

// Macro to get data pointer from a resident attribute pointer. */
#define GetResidentDataPointer(Attrib) (char*)(((ULONG_PTR)Attrib) + \
                                               (Attrib->Resident.DataOffset))

#define GetNamePointer(x) (((char*)x) + (x->NameOffset))

#define GetAttributeDataSize(Attribute1) \
Attribute1->IsNonResident ? Attribute1->NonResident.DataSize : Attribute1->Resident.DataLength

// Macro to free memory from data run.
#define FreeDataRun(x) while(x) {\
    PDataRun tmp = x->NextRun;\
    delete x;\
    x = tmp;\
}

// Macros to get values from a file reference
#define GetFRNFromFileRef(x) (x & 0xFFFFFFFFFFFF)
#define GetSQNFromFileRef(x) ((x << 48) >> 48) & 0xFFFF

/* *** $ATTRDEF ENTRIES *** */

// Collation Rules
#define ATTRDEF_COLLATION_BINARY      0x00
#define ATTRDEF_COLLATION_FILENAME    0x01
#define ATTRDEF_COLLATION_WSTR        0x02
#define ATTRDEF_COLLATION_ULONG       0x10
#define ATTRDEF_COLLATION_SID         0x11
#define ATTRDEF_COLLATION_SEC_HASH    0x12
#define ATTRDEF_COLLATION_ULONG_MULTI 0x13

// Entry Flags
#define ATTRDEF_INDEXED      0x02
#define ATTRDEF_RESIDENT     0x40
#define ATTRDEF_NON_RESIDENT 0x80

typedef struct
{
    WCHAR  Label[64];
    UINT32 AttributeType;
    UINT32 DisplayRule;
    UINT32 CollationRule;
    UINT32 Flags;
    UINT64 MinimumSize;
    UINT64 MaximumSize;
} AttrDefEntry, *PAttrDefEntry;

/* *** EXTENDED ATTRIBUTE HEADERS *** */

// $STANDARD_INFORMATION (0x10)
typedef struct StandardInformationEx
{
    UINT64 CreationTime;                   // Offset 0x00, Size 8
    UINT64 LastWriteTime;                  // Offset 0x08, Size 8
    UINT64 ChangeTime;                     // Offset 0x10, Size 8
    UINT64 LastAccessTime;                 // Offset 0x18, Size 8
    UINT32 FilePermissions;                // Offset 0x20, Size 4
    UINT32 MaxVersions;                    // Offset 0x24, Size 4
    UINT32 VersionNum;                     // Offset 0x28, Size 4
    UINT32 ClassId;                        // Offset 0x2C, Size 4
    UINT32 OwnerId;                        // Offset 0x30, Size 4
    UINT32 SecurityId;                     // Offset 0x34, Size 4
    UINT64 QuotaCharged;                   // Offset 0x38, Size 8
    UINT64 UpdateSequenceNumber;           // Offset 0x40, Size 8
} *PStandardInformationEx;

/// $ATTRIBUTE_LIST (0x20)
typedef struct
{
    UINT32 Type;                           // Offset 0x00, Size 4
    UINT16 RecordLength;                   // Offset 0x04, Size 2
    UINT8  NameLength;                     // Offset 0x06, Size 1
    UINT8  NameOffset;                     // Offset 0x07, Size 1
    UINT64 FirstVCN;                       // Offset 0x08, Size 8
    UINT64 BaseFileRef;                    // Offset 0x10, Size 2
    UINT16 AttributeId;                    // Offset 0x18, Size 2
    UCHAR  Padding[6];                     // Offset 0x1A, Size 6
} AttributeListEx, *PAttributeListEx;

// $FILE_NAME (0x30)
typedef struct
{
    UINT64 ParentFileReference;            // Offset 0x00, Size 8
    UINT64 CreationTime;                   // Offset 0x08, Size 8
    UINT64 LastWriteTime;                  // Offset 0x10, Size 8
    UINT64 ChangeTime;                     // Offset 0x18, Size 8
    UINT64 LastAccessTime;                 // Offset 0x20, Size 8
    UINT64 AllocatedSize;                  // Offset 0x28, Size 8
    UINT64 DataSize;                       // Offset 0x30, Size 8
    UINT32 Flags;                          // Offset 0x38, Size 4
    union
    {
        struct
        {
            USHORT PackedEASize;
            USHORT Padding;
        } EAInfo;
        ULONG ReparseTag;
    } Extended;
    UINT8  NameLength;                     // Offset 0x40, Size 1
    UINT8  NameType;                       // Offset 0x41, Size 1
    WCHAR  Name[1];                        // Offset 0x42, Size variable
} FileNameEx, *PFileNameEx;

// $OBJECT_ID (0x40)
typedef struct ObjectIdEx
{
    GUID ObjectId;                         // Offset 0x00, Size 16
    GUID BirthVolId;                       // Offset 0x10, Size 16
    GUID BirthObjId;                       // Offset 0x20, Size 16
    GUID DomainId;                         // Offset 0x30, Size 16
} ObjectIdEx, *PObjectIdEx;

// $SECURITY_DESCRIPTOR (0x50)
/*struct SECURITY_DESCRIPTOR
{
    // TODO: Complete
};*/

// $VOLUME_INFORMATION (0x70)
typedef struct
{
    UINT64 Reserved1;                      // Offset 0x00, Size 8
    UINT8  MajorVersion;                   // Offset 0x08, Size 1
    UINT8  MinorVersion;                   // Offset 0x09, Size 1
    UINT16 Flags;                          // Offset 0x0A, Size 2
    UINT32 Reserved2;                      // Offset 0x0C, Size 4
} VolumeInformationEx, *PVolumeInformationEx;

// $INDEX_ROOT (0x90)
typedef struct
{
    ULONG  IndexOffset;                    // Offset 0x00, Size 4
    UINT16 TotalIndexSize;                 // Offset 0x04, Size 4
    UINT16 Unknown;
    ULONG  AllocatedSize;                  // Offset 0x08, Size 4
    UCHAR  Flags;                          // Offset 0x0C, Size 1
    UCHAR  Padding[3];                     // Offset 0x0D, Size 3
} IndexNodeHeader, *PIndexNodeHeader;

typedef struct
{
    ULONG AttributeType;                  // Offset 0x00, Size 4
    ULONG CollationRule;                  // Offset 0x04, Size 4
    ULONG BytesPerIndexRec;               // Offset 0x08, Size 4
    UCHAR ClusPerIndexRec;                // Offset 0x0C, Size 1
    UCHAR Padding[3];                     // Offset 0x0D, Size 3
    IndexNodeHeader Header;               // Offset 0x10, Size 16
} IndexRootEx, *PIndexRootEx;

typedef struct
{
    union
    {
        struct                             // Offset 0x00, Size 8
        {
            ULONGLONG IndexedFile;
        } Directory;
        struct
        {
            USHORT    DataOffset;
            USHORT    DataLength;
            ULONG     Reserved;
        } ViewIndex;
    } Data;
    UINT16     EntryLength;                   // Offset 0x08, Size 2
    UINT16     StreamLength;                  // Offset 0x0A, Size 2
    UINT8      Flags;                         // Offset 0x0C, Size 1
    UINT16     Reserved;
    UCHAR      IndexStream[1];                // Offset 0x10, Size varies
} IndexEntry, *PIndexEntry;

// $REPARSE_POINT (0xC0)
typedef struct
{
    UINT32 ReparseType;                    // Offset 0x00, Size 4
    UINT16 ReparseDataLength;              // Offset 0x04, Size 2
    UINT16 Padding;                        // Offset 0x06, Size 2
} ReparsePointEx, *PReparsePointEx;

// TODO: Simplify this so we're not defining ReparsePointEx twice.
typedef struct _ThirdPartyReparsePointEx
{
    UINT32 ReparseType;                    // Offset 0x00, Size 4
    UINT16 ReparseDataLength;              // Offset 0x04, Size 2
    UINT16 Padding;                        // Offset 0x06, Size 2
    GUID ReparseGUID;                      // Offset 0x08, Size 16
} ThirdPartyReparsePointEx, *PThirdPartyReparsePointEx;

// $EA_INFORMATION (0xD0)
typedef struct
{
    UINT16 PackedEASize;                   // Offset 0x00, Size 2
    UINT16 NumEAWithNEED_EA;               // Offset 0x02, Size 2
    UINT32 UnpackedEASize;                 // Offset 0x04, Size 4
} EAInformationEx, *PEAInformationEx;

// $EA (0xE0)
typedef struct
{
    UINT32 OffsetNextEA;                   // Offset 0x00, Size 4
    UINT8  Flags;                          // Offset 0x04, Size 1
    UINT8  NameLength;                     // Offset 0x05, Size 1
    UINT16 ValueLength;                    // Offset 0x06, Size 2
} EAEx, *PEAEx;


#pragma pack(1)
struct BootSector
{
    UCHAR  JumpInstruction[3];     // Offset 0x00, Size 3
    UCHAR  OEM_ID[8];              // Offset 0x03, Size 8
    UINT16 BytesPerSector;         // Offset 0x0B, Size 2
    UINT8  SectorsPerCluster;      // Offset 0x0D, Size 1
    UCHAR  Reserved0[7];           // Offset 0x0E, Size 7
    UINT8  MediaDescriptor;        // Offset 0x15, Size 1
    UCHAR  Reserved1[2];           // Offset 0x16, Size 2
    UINT16 SectorsPerTrack;        // Offset 0x18, Size 2
    UINT16 NumberOfHeads;          // Offset 0x1A, Size 2
    UCHAR  Reserved2[4];           // Offset 0x1C, Size 4
    UCHAR  Reserved3[4];           // Offset 0x20, Size 4
    UINT32 Unknown;                // Offset 0x24, Size 4
    UINT64 SectorsInVolume;        // Offset 0x28, Size 8
    UINT64 MFTLCN;                 // Offset 0x30, Size 8
    UINT64 MFTMirrLCN;             // Offset 0x38, Size 8
    INT8   ClustersPerFileRecord;  // Offset 0x40, Size 4
    UCHAR  Reserved4[3];
    INT8   ClustersPerIndexRecord; // Offset 0x44, Size 4
    UCHAR  Reserved5[3];
    UINT64 SerialNumber;           // Offset 0x48, Size 8
    UINT32 Checksum;               // Offset 0x50, Size 4
    UCHAR  BootStrap[426];
    USHORT EndSector;
};

typedef struct BootSector BootSector;
typedef struct BootSector* PBootSector;

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

/* NTFS file permission flags */
#define FILE_PERM_READONLY   0x0001
#define FILE_PERM_HIDDEN     0x0002
#define FILE_PERM_SYSTEM     0x0004
#define FILE_PERM_ARCHIVE    0x0020
#define FILE_PERM_DEVICE     0x0040
#define FILE_PERM_NORMAL     0x0080
#define FILE_PERM_TEMP       0x0100
#define FILE_PERM_SPARSE     0x0200
#define FILE_PERM_REPARSE_PT 0x0400
#define FILE_PERM_COMPRESSED 0x0800
#define FILE_PERM_OFFLINE    0x1000
#define FILE_PERM_NOT_INDXED 0x2000
#define FILE_PERM_ENCRYPTED  0x4000

// Forward declarations for DataRun struct because it's a linked list.
struct DataRun;
typedef struct DataRun *PDataRun;

struct DataRun
{
    PDataRun  NextRun;
    ULONGLONG LCN;
    ULONGLONG Length; // In clusters
};

typedef struct
{
    UCHAR  TypeID[4];              // Offset 0x00, Size 4 ('FILE' or 'INDX')
    UINT16 UpdateSequenceOffset;   // Offset 0x04, Size 2
    UINT16 SizeOfUpdateSequence;   // Offset 0x06, Size 2
    UINT64 LogFileSequenceNumber;  // Offset 0x08, Size 8
} NTFSRecordHeader, *PNTFSRecordHeader;

typedef struct
{
    NTFSRecordHeader Header;       // Offset 0x00, Size 16
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
} FileRecordHeader, *PFileRecordHeader;

#define INDEX_ENTRY_NODE 1
#define INDEX_ENTRY_END  2

struct _BTreeNode;
struct _BTreeKey;

typedef struct _BTreeNode BTreeNode, *PBTreeNode;
typedef struct _BTreeKey BTreeKey, *PBTreeKey;

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
    PIndexEntry Entry;
    ULONG       Flags;
};

typedef struct
{
    NTFSRecordHeader RecordHeader;
    ULONGLONG VCN;
    IndexNodeHeader IndexHeader;
} IndexBuffer, *PIndexBuffer;

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
    ULONG Flags;
    ULONG OpenHandleCount;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;
    PVPB VolParamBlock;
    PFILE_OBJECT PubFileObject;
    PDEVICE_OBJECT PartDeviceObj;
    class MasterFileTable* MFT;
    class LogFileService* LFS;
    BOOLEAN IsReadOnly = FALSE;

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
    GetADSPreference(_In_  PFILE_OBJECT FileObj,
                     _Out_ AttributeType* RequestedType,
                     _Out_ PWSTR* RequestedStream);

    // ./sanity.cpp
    // These functions will likely be removed before the driver is released.
    void RunSanityChecks();
    void SanityCheckBlockIO();

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

typedef class BTree
{
public:
    NTSTATUS ResetCurrentKey();
    // Hack:
    // TODO: Make private when we abandon oldbtreefuncs
    PBTreeNode RootNode;
protected:
    ~BTree();
    PBTreeKey CurrentKey;
} *PBTree;

typedef class Directory : BTree
{
public:
    // ./directory.cpp
    Directory(_In_ PVolume DiskVolume);
    Directory(_In_ PVolume DiskVolume,
              _In_ PFileRecord File);
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
    VerifyUpdateSequenceArray(PNTFSRecordHeader Record);
    NTSTATUS
    CreateNode(_In_    PFileRecord File,
               _In_    PAttribute  IndexAllocationAttribute,
               _Inout_ PBTreeKey   ParentNodeKey);
    NTSTATUS
    CreateRootNode(_In_  PFileRecord File,
                   _Out_ PBTreeNode *NewRootNode);
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

    // ./ mft.cpp
    MasterFileTable(_In_ PVolume TargetVolume,
                    _In_ UINT64 MFTLCN,
                    _In_ UINT64 MFTMirrLCN,
                    _In_ INT8   ClustersPerFileRecord);

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
    INT    MftZoneReservation;
    PFileRecord MFTFile = NULL;
    PFileRecord MFTMirrFile = NULL;
} *PMasterFileTable;
#endif // __cplusplus

// =========================
// NTFS C API
// =========================

/* Forward declarations */
typedef struct NtfsDirectory NtfsDirectory;
typedef NtfsDirectory* PNtfsDirectory;

typedef struct NtfsFileRecord NtfsFileRecord;
typedef NtfsFileRecord* PNtfsFileRecord;

typedef struct NtfsLogFileService NtfsLogFileService;
typedef NtfsLogFileService* PNtfsLogFileService;

typedef struct NtfsMasterFileTable NtfsMasterFileTable;
typedef NtfsMasterFileTable* PNtfsMasterFileTable;

typedef struct NtfsVolume NtfsVolume;
typedef NtfsVolume* PNtfsVolume;

#ifdef __cplusplus
extern "C" {
#endif

/* Directory functions */
PNtfsDirectory
NtfsDirectoryCreate(
    _In_ PNtfsVolume DiskVolume);

PNtfsDirectory
NtfsDirectoryCreateEx(
    _In_ PNtfsVolume DiskVolume,
    _In_ PNtfsFileRecord File);

NTSTATUS
NTAPI
NtfsDirectoryGetFileBothDirInfo(
    _In_    PNtfsDirectory Dir,
    _In_    BOOLEAN ReturnSingleEntry,
    _In_    BOOLEAN RestartScan,
    _In_    PUNICODE_STRING FileNameFilter,
    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
    _Inout_ PULONG BufferLength);

NTSTATUS
NtfsDirectoryLoadDirectory(
    _In_ PNtfsDirectory Dir,
    _In_ PNtfsFileRecord File);

/* FileRecord functions */
PNtfsFileRecord
NtfsFileRecordCreate(
    _In_ void *DiskVolume,
    _In_ ULONG FileRecordSize);

void
NtfsFileRecordDestroy(
    _In_opt_ NtfsFileRecord *FileRecord);

PFileRecordHeader
NTAPI
NtfsFileRecordGetHeader(
    _In_ NtfsFileRecord *FileRecord);

PUCHAR
NTAPI
NtfsFileRecordGetData(
    _In_ NtfsFileRecord *FileRecord);

PAttribute
NTAPI
NtfsFileRecordGetAttribute(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name);

PDataRun
NTAPI
NtfsFileRecordFindNonResidentDataFromAttribute(
    _In_ NtfsFileRecord *FileRecord,
    _In_ PAttribute DataAttr);

PDataRun
NTAPI
NtfsFileRecordFindNonResidentData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name);

NTSTATUS
NTAPI
NtfsFileRecordCopyData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset);

NTSTATUS
NTAPI
NtfsFileRecordCopyDataFromAttribute(
    _In_ NtfsFileRecord *FileRecord,
    _In_ PAttribute Attr,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset);

NTSTATUS
NTAPI
NtfsFileRecordWriteFileData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType AttrType,
    _In_opt_ PWSTR StreamName,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ PLARGE_INTEGER Offset);

NTSTATUS
NTAPI
NtfsFileRecordUpdateResidentData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ PAttribute TargetAttribute,
    _In_ PUCHAR Buffer,
    _In_ PULONG Length,
    _In_ ULONGLONG Offset);

NTSTATUS
NTAPI
NtfsFileRecordCommitFixup(
    _In_ NtfsFileRecord *FileRecord);

NTSTATUS
NTAPI
NtfsFileRecordApplyFixup(
    _In_ NtfsFileRecord *FileRecord);

/* LFS functions */
NTSTATUS
NtfsLogFileServiceGetClientMajorVersion(
    _In_ PNtfsLogFileService LFS);

NTSTATUS
NtfsLogFileServiceGetClientMinorVersion(
    _In_ PNtfsLogFileService LFS);

/* MFT functions */
NTSTATUS
NtfsMasterFileTableGetFileRecordFromQuery(
    _In_ PNtfsMasterFileTable Mft,
    _In_ PWCHAR Query,
    _Out_ PNtfsFileRecord* File);

/* Probe functions */
NTSTATUS
NtfsProbePartition(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData);

NTSTATUS
NtfsProbePartitionAndOpenVolume(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData,
    _Out_ void** VolumeOut);

/* Volume functions */
NTSTATUS
NtfsVolumeGetADSPreference(
    _In_ PNtfsVolume DiskVolume,
    _In_ PFILE_OBJECT FileObject,
    _Out_ AttributeType* RequestedType,
    _Out_ PWSTR* RequestedStream);

UINT8
NtfsVolumeGetBytesPerSector(
    _In_ PNtfsVolume DiskVolume);

ULONG
NtfsVolumeGetClustersInVolume(
    _In_ PNtfsVolume DiskVolume);

NTSTATUS
NtfsVolumeGetFreeClusters(
    _In_ PNtfsVolume DiskVolume,
    _Out_ PLARGE_INTEGER FreeClusters);

PNtfsLogFileService
NtfsVolumeGetLFS(
    _In_ PNtfsVolume DiskVolume);

PNtfsMasterFileTable
NtfsVolumeGetMft(
    _In_ PNtfsVolume DiskVolume);

USHORT
NtfsVolumeGetMajorVersion(
    _In_ PNtfsVolume DiskVolume);

USHORT
NtfsVolumeGetMinorVersion(
    _In_ PNtfsVolume DiskVolume);

UINT8
NtfsVolumeGetSectorsPerCluster(
    _In_ PNtfsVolume DiskVolume);

UINT64
NtfsVolumeGetSerialNumber(
    _In_ PNtfsVolume DiskVolume);

NTSTATUS
NtfsVolumeGetVolumeLabel(
    _In_ PNtfsVolume DiskVolume,
    _Out_ PWSTR VolumeLabel,
    _Out_ PUSHORT Length);

BOOLEAN
NtfsVolumeIsReadOnly(
    _In_ PNtfsVolume DiskVolume);

NTSTATUS
NtfsVolumeSetVolumeLabel(
    _In_ PNtfsVolume DiskVolume,
    _In_ PWSTR VolumeLabel,
    _In_ ULONG Length);

#ifdef __cplusplus
}
#endif
