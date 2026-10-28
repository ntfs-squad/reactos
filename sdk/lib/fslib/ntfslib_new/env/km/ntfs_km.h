
#define NTFS_KERNEL_MODE

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG SectorBytes);

/* Hack: for other hacks */
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

typedef enum AttributeType AttributeType;

/* HACK: This is only temporary as I refactor this out of the public API. */
NTSTATUS
NtfsDirectoryGetFileBothDirInfo(
    _In_    PNtfsDirectory Dir,
    _In_    BOOLEAN ReturnSingleEntry,
    _In_    BOOLEAN RestartScan,
    _In_    PUNICODE_STRING FileNameFilter,
    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
    _Inout_ PULONG BufferLength);

/* HACK: This is only temporary as I refactor this out of the public API. */
NTSTATUS
NtfsVolumeGetADSPreference(
    _In_ PNtfsVolume DiskVolume,
    _In_ PFILE_OBJECT FileObject,
    _Out_ AttributeType* RequestedType,
    _Out_ PWSTR* RequestedStream);

#ifdef __cplusplus
}
#endif

