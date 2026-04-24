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

// =========================
// NTFS Memory Tags
// =========================
// HACK: These should be private or in *km target.

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

#include "attributes.h"
#include "ntfsvol.h"
#include "filerecord.h"
#include "btree.h"
#include "capi.h"


// =========================
// NTFS Classes
// =========================

#ifdef __cplusplus


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

EXTERN_C
NTSTATUS
NtfsProbePartition(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData);

EXTERN_C
NTSTATUS
NtfsProbePartitionAndOpenVolume(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData,
    _Out_ void** VolumeOut);

// TODO: Maybe we should remove this if NTFS_DEBUG isn't defined?
#include <debug.h>
#include "ntfsdbg.h"
