/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#ifndef _NTFSPROCS_
#define _NTFSPROCS_

#include <ntifs.h>

#include <ntddscsi.h>
#include <scsi.h>
#include <ntddcdrm.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include <ntintsafe.h>
#include <pseh/pseh2.h>

#define TAG_NTFS 'NTFS'
#define NTFS_DEBUG

#define GetWStrLength(x) x * sizeof(WCHAR)
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))
#define ULONG_ROUND_UP(x)   ROUND_UP((x), (sizeof(ULONG)))
#define MAX_SHORTNAME_LENGTH 12
#define FileRef(x) x->Entry->Data.Directory.IndexedFile

typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile,
} TYPE_OF_OPEN;

/* ntfsinit.cpp */

_Function_class_(DRIVER_UNLOAD)
EXTERN_C
VOID
NTAPI
NtfsUnload(_In_ _Unreferenced_parameter_ PDRIVER_OBJECT DriverObject);

EXTERN_C
NTSTATUS
NTAPI
DriverEntry(_In_ PDRIVER_OBJECT DriverObject,
            _In_ PUNICODE_STRING RegistryPath);

_Function_class_(IRP_MJ_CLEANUP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCleanup (_In_    PDEVICE_OBJECT VolumeDeviceObject,
                _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_LOCK_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdLockControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                   _Inout_ PIRP Irp)

_Function_class_(IRP_MJ_DEVICE_CONTROL)
_Function_class_(DRIVER_DISPATCH);
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDeviceControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SHUTDOWN)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdShutdown (_In_    PDEVICE_OBJECT VolumeDeviceObject,
                 _Inout_ PIRP Irp);
/* fastio */
BOOLEAN NTAPI
NtfsAcqLazyWrite(PVOID Context,
                 BOOLEAN Wait);

VOID NTAPI
NtfsRelLazyWrite(PVOID Context);

BOOLEAN NTAPI
NtfsAcqReadAhead(PVOID Context,
                 BOOLEAN Wait);

VOID NTAPI
NtfsRelReadAhead(PVOID Context);

/* fsctrl */
_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsCommonFileSystemControl(_In_ PIRP Irp);

_Function_class_(IRP_MJ_FILE_SYSTEM_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFileSystemControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                         _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_FLUSH_BUFFERS)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFlushBuffers (_In_    PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp);

/* create.cpp */
_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCreate(_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

/* close.cpp  */
_Function_class_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdClose (_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

/* read.cpp */
_Function_class_(IRP_MJ_READ)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdRead(_In_    PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

/* write.cpp */
_Function_class_(IRP_MJ_WRITE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdWrite (_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

/* fileinfo.cpp */

_Function_class_(IRP_MJ_QUERY_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_DIRECTORY_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDirectoryControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SET_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp);
/* ea.cpp */
_Function_class_(IRP_MJ_QUERY_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryEa(_In_   PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SET_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetEa(_In_   PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

/* vol.cpp */
_Function_class_(IRP_MJ_QUERY_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryVolumeInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                              _Inout_ PIRP Irp);

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject);

_Function_class_(IRP_MJ_SET_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetVolumeInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                            _Inout_ PIRP Irp);
/* pnp.cpp */
_Function_class_(IRP_MJ_PNP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdPnp(_In_    PDEVICE_OBJECT VolumeDeviceObject,
           _Inout_ PIRP Irp);

/* ntblockio.cpp*/
NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceToRead,
         _In_    LONGLONG Offset,
         _In_    ULONG Length,
         _Inout_ PUCHAR Buffer);

NTSTATUS
ReadDiskUnaligned(_In_    PDEVICE_OBJECT DeviceToRead,
                  _In_    LONGLONG Offset,
                  _In_    ULONG Length,
                  _Inout_ PUCHAR Buffer);

NTSTATUS
ReadBlock(_In_    PDEVICE_OBJECT DeviceObject,
          _In_    ULONG DiskSector,
          _In_    ULONG SectorCount,
          _In_    ULONG SectorSize,
          _Inout_ PUCHAR Buffer);

NTSTATUS
WriteDisk(_In_ PDEVICE_OBJECT DeviceBeingRead,
          _In_ LONGLONG StartingOffset,
          _In_ ULONG AmountOfBytes,
          _In_ PUCHAR BufferToWrite);

NTSTATUS
WriteBlock(_In_   PDEVICE_OBJECT DeviceObject,
          _In_    ULONG DiskSector,
          _In_    ULONG SectorCount,
          _In_    ULONG SectorSize,
          _Inout_ PUCHAR Buffer);
NTSTATUS
DeviceIoControl(_In_    PDEVICE_OBJECT DeviceObject,
                _In_    ULONG ControlCode,
                _In_    PVOID InputBuffer,
                _In_    ULONG InputBufferSize,
                _Inout_ PVOID OutputBuffer,
                _Inout_ PULONG OutputBufferSize,
                _In_    BOOLEAN Override);

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);

#include "../filerecord/attributes.h"
#include "contextblocks.h"
#include "../ntfsvol.h"
#include "../filerecord/filerecord.h"
#include "../btree/btree.h"
#include "../directory/directory.h"
#include "../mft/mft.h"

#include <debug.h>

#ifdef NTFS_DEBUG

#define PrintFlag(Item, Flag, FlagName) if(Item & Flag) \
DbgPrint("    %s\n", FlagName); \
/* Debug print functions. REMOVE WHEN DONE. */
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

static inline void PrintNTFSBootSector(BootSector* PartBootSector)
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

static inline void PrintFileContextBlock(PFileContextBlock FileCB)
{
    DbgPrint("File Record Number: %lu\n", FileCB->FileRecordNumber);
    DbgPrint("File name:          \"%S\"\n", FileCB->FileName);

    if (FileCB->IsDirectory)
        DbgPrint("Directory:          TRUE\n");
    else
        DbgPrint("Directory:          FALSE\n");

    DbgPrint("Number of Links:    %lu\n", FileCB->NumberOfLinks);
    DbgPrint("Change Time:        %lu\n", FileCB->ChangeTime);
    DbgPrint("Last Access Time:   %lu\n", FileCB->LastAccessTime);
    DbgPrint("Last Write Time:    %lu\n", FileCB->LastWriteTime);
    DbgPrint("Creation Time:      %lu\n", FileCB->CreationTime);
};

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
#endif

#endif // _NTFSPROCS_
