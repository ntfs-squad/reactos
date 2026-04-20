/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header for MFT Implementation
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#define GetOffset(LCN) (LCN * BytesPerCluster(Volume))
#define GetRunSize(Run) (Run->Length * BytesPerCluster(Volume))

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

#ifdef __cplusplus

typedef class FileRecord
{
public:
    PFileRecordHeader Header;
    PUCHAR Data = NULL;

    // ./filerecord.cpp
    FileRecord(_In_ PNTFSVolume Volume,
               _In_ ULONG FileRecordSize);
    FileRecord(_In_ PNTFSVolume Volume);
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
    PNTFSVolume Volume;

    // ./write.cpp
    NTSTATUS
    UpdateNonResidentData(_In_ PAttribute TargetAttribute,
                          _In_ PUCHAR Buffer,
                          _In_ PULONG Length,
                          _In_ ULONGLONG Offset = 0);
} *PFileRecord;

#endif // __cplusplus

// Needed for C API
typedef struct NtfsFileRecord NtfsFileRecord;
typedef NtfsFileRecord* PNtfsFileRecord;

#ifdef __cplusplus
extern "C" {
#endif

PNtfsFileRecord
NtfsFileRecordCreate(
    _In_ void *Volume,
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

#ifdef __cplusplus
}
#endif
