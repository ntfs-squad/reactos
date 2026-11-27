
#ifndef _WINNT_
#include <ntdef.h>
#endif

// Hack: we shouldn't be defining UINT, BOOL.
#ifndef UINT
typedef unsigned int UINT;
#endif
#ifndef BOOL
typedef int BOOL;
#endif

#define ASSERT_FIELD_OFFSET_AND_LENGTH(_Struct, _Field, _Offset, _Length) \
    C_ASSERT(offsetof(_Struct, _Field) == _Offset);                       \
    C_ASSERT(sizeof(((_Struct *)0)->_Field) == _Length)

#include "ntfsattribdef.h"

#pragma pack(push, 1)
typedef struct _BootSector
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
    INT8   ClustersPerFileRecord;  // Offset 0x40, Size 1
    UCHAR  Reserved4[3];           // Offset 0x41, Size 3
    INT8   ClustersPerIndexRecord; // Offset 0x44, Size 1
    UCHAR  Reserved5[3];           // Offset 0x45, Size 3
    UINT64 SerialNumber;           // Offset 0x48, Size 8
    UINT32 Checksum;               // Offset 0x50, Size 4
    UCHAR  BootStrap[426];         // Offset 0x54, Size 426
    USHORT EndSector;              // Offset 0x1FE, Size 2
} BootSector, *PBootSector;
#pragma pack(pop)
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, JumpInstruction,        0x00, 3);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, OEM_ID,                 0x03, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, BytesPerSector,         0x0B, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, SectorsPerCluster,      0x0D, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Reserved0,              0x0E, 7);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, MediaDescriptor,        0x15, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Reserved1,              0x16, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, SectorsPerTrack,        0x18, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, NumberOfHeads,          0x1A, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Reserved2,              0x1C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Reserved3,              0x20, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Unknown,                0x24, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, SectorsInVolume,        0x28, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, MFTLCN,                 0x30, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, MFTMirrLCN,             0x38, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, ClustersPerFileRecord,  0x40, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Reserved4,              0x41, 3);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, ClustersPerIndexRecord, 0x44, 1);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Reserved5,              0x45, 3);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, SerialNumber,           0x48, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, Checksum,               0x50, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, BootStrap,              0x54, 426);
ASSERT_FIELD_OFFSET_AND_LENGTH(BootSector, EndSector,              0x1FE, 2);
C_ASSERT(sizeof(BootSector) == 0x200);

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

typedef struct _NTFSRecordHeader
{
    UCHAR  TypeID[4];              // Offset 0x00, Size 4 ('FILE' or 'INDX')
    UINT16 UpdateSequenceOffset;   // Offset 0x04, Size 2
    UINT16 SizeOfUpdateSequence;   // Offset 0x06, Size 2
    UINT64 LogFileSequenceNumber;  // Offset 0x08, Size 8
} NTFSRecordHeader, *PNTFSRecordHeader;
ASSERT_FIELD_OFFSET_AND_LENGTH(NTFSRecordHeader, TypeID,                0x00, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(NTFSRecordHeader, UpdateSequenceOffset,  0x04, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NTFSRecordHeader, SizeOfUpdateSequence,  0x06, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(NTFSRecordHeader, LogFileSequenceNumber, 0x08, 8);

typedef struct _FileRecordHeader
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
    UINT16 Padding;                // Offset 0x2A, Size 2
    UINT32 MFTRecordNumber;        // Offset 0x2C, Size 4
} FileRecordHeader, *PFileRecordHeader;
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, Header,          0x00, 16);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, SequenceNumber,  0x10, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, HardLinkCount,   0x12, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, AttributeOffset, 0x14, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, Flags,           0x16, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, ActualSize,      0x18, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, AllocatedSize,   0x1C, 4);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, BaseFileRecord,  0x20, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, NextAttributeID, 0x28, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, Padding,         0x2A, 2);
ASSERT_FIELD_OFFSET_AND_LENGTH(FileRecordHeader, MFTRecordNumber, 0x2C, 4);

typedef struct _IndexBuffer
{
    NTFSRecordHeader RecordHeader; // Offset 0x00, Size 16
    ULONGLONG VCN;                 // Offset 0x10, Size 8
    IndexNodeHeader IndexHeader;   // Offset 0x18, Size 16
} IndexBuffer, *PIndexBuffer;
ASSERT_FIELD_OFFSET_AND_LENGTH(IndexBuffer, RecordHeader, 0x00, 16);
ASSERT_FIELD_OFFSET_AND_LENGTH(IndexBuffer, VCN,          0x10, 8);
ASSERT_FIELD_OFFSET_AND_LENGTH(IndexBuffer, IndexHeader,  0x18, 16);

typedef struct _NtfsMasterFileTableInfo
{
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    UINT64 MFTZoneEndLCN;
    UINT8  ZoneReservationMultiplier;
} NtfsMasterFileTableInfo, *PNtfsMasterFileTableInfo;

typedef struct _NtfsConfiguration
{
    BOOL IsReadOnly;
    INT  MftZoneReservation;
    BOOL ShowHiddenMetadataFiles;
    BOOL UseLegacyMftZoneScaling;
} NtfsConfiguration, *PNtfsConfiguration;

typedef struct _NtfsVersion
{
    ULONG MajorVersion;
    ULONG MinorVersion;
} NtfsVersion, *PNtfsVersion;

typedef NtfsVersion NtfsLfsClientVersion, *PNtfsLfsClientVersion;

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

VOID
NtfsDirectoryDestroy(
    _In_opt_ PNtfsDirectory Dir);

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
NtfsFileRecordGetHeader(
    _In_ NtfsFileRecord *FileRecord);

PUCHAR
NtfsFileRecordGetData(
    _In_ NtfsFileRecord *FileRecord);

PAttribute
NtfsFileRecordGetAttribute(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name);

NTSTATUS
NtfsFileRecordGetAttributeData(
    _In_     PNtfsFileRecord Fr,
    _In_     AttributeType Type,
    _In_opt_ PWSTR Name,
    _Out_    PUCHAR *Data);

PDataRun
NtfsFileRecordFindNonResidentDataFromAttribute(
    _In_ NtfsFileRecord *FileRecord,
    _In_ PAttribute DataAttr);

PDataRun
NtfsFileRecordFindNonResidentData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name);

NTSTATUS
NtfsFileRecordCopyData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset);

NTSTATUS
NtfsFileRecordCopyDataFromAttribute(
    _In_ NtfsFileRecord *FileRecord,
    _In_ PAttribute Attr,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset);

NTSTATUS
NtfsFileRecordWriteFileData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType AttrType,
    _In_opt_ PWSTR StreamName,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ PLARGE_INTEGER Offset);

NTSTATUS
NtfsFileRecordUpdateResidentData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ PAttribute TargetAttribute,
    _In_ PUCHAR Buffer,
    _In_ PULONG Length,
    _In_ ULONGLONG Offset);

NTSTATUS
NtfsFileRecordCommitFixup(
    _In_ NtfsFileRecord *FileRecord);

NTSTATUS
NtfsFileRecordApplyFixup(
    _In_ NtfsFileRecord *FileRecord);

/* Format functions */
// NTSTATUS
// NtfsFormatPartition();

/* LFS functions */
VOID
NtfsLogFileServiceGetClientVersion(
    _In_ PNtfsLogFileService Lfs,
    _Out_ PNtfsLfsClientVersion ClientVersion);

/* MFT functions */
NTSTATUS
NtfsMasterFileTableGetMftInformation(
    _In_ PNtfsMasterFileTable Mft,
    _Out_ PNtfsMasterFileTableInfo MftInformation);

NTSTATUS
NtfsMasterFileTableGetFileRecordFromQuery(
    _In_ PNtfsMasterFileTable Mft,
    _In_ PWCHAR Query,
    _Out_ PNtfsFileRecord* File);

NTSTATUS
NtfsMasterFileTableGetFileRecord(
    _In_  PNtfsMasterFileTable Mft,
    _In_  ULONGLONG FileRecordNumber,
    _Out_ PNtfsFileRecord* File);

typedef struct _NtfsDevice
{
    ULONG BytesPerSector;
    PVOID* Internal;
} NtfsDevice, *PNtfsDevice;

NTSTATUS
NtfsProbePartition(
    _In_      PNtfsDevice Device,
    _In_opt_  PNtfsConfiguration Configuration,
    _Out_opt_ PNtfsVolume* VolumeOut,
    _Out_opt_ PBootSector BootSectorOut);

/* Volume functions */
ULONG
NtfsVolumeGetBytesPerSector(
    _In_ PNtfsVolume DiskVolume);

UINT8
NtfsVolumeGetSectorsPerCluster(
    _In_ PNtfsVolume DiskVolume);

ULONG
NtfsVolumeGetClustersInVolume(
    _In_ PNtfsVolume DiskVolume);

ULONGLONG
NtfsVolumeGetSize(
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

VOID
NtfsVolumeGetVersion(
    _In_ PNtfsVolume DiskVolume,
    _Out_ PNtfsVersion NtfsVersion);

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

// =========================
// NTFS C++ Classes
// =========================
#ifdef __cplusplus
// TODO: Add Pimpl classes for Volume, MasterFileTable, LogFileService, Directory, FileRecord.
#endif
