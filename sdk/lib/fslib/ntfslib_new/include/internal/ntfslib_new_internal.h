


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

/* Private functions */
NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer);

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer);

#include "lfs.h"
