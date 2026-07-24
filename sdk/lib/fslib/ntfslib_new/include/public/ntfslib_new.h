
#ifndef _NTFSLIB_NEW_H_
#define _NTFSLIB_NEW_H_

#ifndef _WINNT_
#include <ntdef.h>
#endif

#include "ntfsattribdef.h"

#pragma pack(push, 1)
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
#pragma pack(pop)
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

/* File records up to and including this number are reserved for
 * NTFS metadata files and are hidden from directory enumeration.
 */
#define NTFS_LAST_RESERVED_FILE_RECORD 26

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

typedef struct
{
    NTFSRecordHeader RecordHeader;
    ULONGLONG VCN;
    IndexNodeHeader IndexHeader;
} IndexBuffer, *PIndexBuffer;

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

void
NtfsDirectoryDestroy(
    _In_opt_ PNtfsDirectory Dir);

NTSTATUS
NtfsDirectoryLoadDirectory(
    _In_ PNtfsDirectory Dir,
    _In_ PNtfsFileRecord File);

/* FileRecord functions */
PNtfsFileRecord
NtfsFileRecordCreate(
    _In_ PNtfsVolume DiskVolume,
    _In_ ULONG FileRecordSize);

void
NtfsFileRecordDestroy(
    _In_opt_ NtfsFileRecord *FileRecord);

PFileRecordHeader
NtfsFileRecordGetHeader(
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

NTSTATUS
NtfsFileRecordCopyData(
    _In_ NtfsFileRecord *FileRecord,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name,
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

/* LFS functions */
ULONG
NtfsLogFileServiceGetClientMajorVersion(
    _In_ PNtfsLogFileService LFS);

ULONG
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

/* Reads the boot sector through the environment's disk routines
 * (initialize them first) and mounts the volume if it is NTFS.
 */
NTSTATUS
NtfsProbePartitionAndOpenVolume(
    _In_ ULONG BytesPerSector,
    _Out_ PNtfsVolume* VolumeOut);

/* Library options */
void
NtfsSetShowMetadataFiles(
    _In_ BOOLEAN Show);

/* Volume functions */
NTSTATUS
NtfsVolumeGetADSPreference(
    _In_ PNtfsVolume DiskVolume,
    _In_ PUNICODE_STRING FileName,
    _Out_ AttributeType* RequestedType,
    _Out_ PWSTR* RequestedStream);

ULONG
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

#endif /* _NTFSLIB_NEW_H_ */
