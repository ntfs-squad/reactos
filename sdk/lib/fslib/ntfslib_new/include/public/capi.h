
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

/* Functions */
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
