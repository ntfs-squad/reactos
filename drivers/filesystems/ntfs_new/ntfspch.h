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
#define TAG_MFT '$MFT'
#define TAG_FILE_RECORD 'FREC'
#define TAG_DATA_RUN 'DTRN'
#define TAG_LOG_FILE_SERVICE 'LgFS'
#define TAG_BTREE 'BTRE'
#define NTFS_DEBUG

#define GetWStrLength(x) x * sizeof(WCHAR)
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))
#define ULONG_ROUND_UP(x)   ROUND_UP((x), (sizeof(ULONG)))
#define MAX_SHORTNAME_LENGTH 12
#define FileRef(Key) (Key)->Entry->Data.Directory.IndexedFile
#define GetUserBuffer(Irp) Irp->MdlAddress ?\
MmGetSystemAddressForMdlSafe(Irp->MdlAddress, ((Irp->Flags & IRP_PAGING_IO) ? HighPagePriority : NormalPagePriority)) :\
Irp->UserBuffer
#define GetBuffer(Irp) Irp->AssociatedIrp.SystemBuffer ? Irp->AssociatedIrp.SystemBuffer : GetUserBuffer(Irp)

// TODO: Why don't we have these status codes?
#ifdef __REACTOS__
#define STATUS_LOG_SECTOR_INVALID            ((NTSTATUS)0xC01A0001L)
#define STATUS_LOG_SECTOR_PARITY_INVALID     ((NTSTATUS)0xC01A0002L)
#define STATUS_LOG_SECTOR_REMAPPED           ((NTSTATUS)0xC01A0003L)
#define STATUS_LOG_BLOCK_INCOMPLETE          ((NTSTATUS)0xC01A0004L)
#define STATUS_LOG_INVALID_RANGE             ((NTSTATUS)0xC01A0005L)
#define STATUS_LOG_BLOCKS_EXHAUSTED          ((NTSTATUS)0xC01A0006L)
#define STATUS_LOG_READ_CONTEXT_INVALID      ((NTSTATUS)0xC01A0007L)
#define STATUS_LOG_RESTART_INVALID           ((NTSTATUS)0xC01A0008L)
#define STATUS_LOG_BLOCK_VERSION             ((NTSTATUS)0xC01A0009L)
#define STATUS_LOG_BLOCK_INVALID             ((NTSTATUS)0xC01A000AL)
#define STATUS_LOG_READ_MODE_INVALID         ((NTSTATUS)0xC01A000BL)
#define STATUS_LOG_NO_RESTART                ((NTSTATUS)0x401A000CL)
#define STATUS_LOG_METADATA_CORRUPT          ((NTSTATUS)0xC01A000DL)
#define STATUS_LOG_METADATA_INVALID          ((NTSTATUS)0xC01A000EL)
#define STATUS_LOG_METADATA_INCONSISTENT     ((NTSTATUS)0xC01A000FL)
#define STATUS_LOG_RESERVATION_INVALID       ((NTSTATUS)0xC01A0010L)
#define STATUS_LOG_CANT_DELETE               ((NTSTATUS)0xC01A0011L)
#define STATUS_LOG_CONTAINER_LIMIT_EXCEEDED  ((NTSTATUS)0xC01A0012L)
#define STATUS_LOG_START_OF_LOG              ((NTSTATUS)0xC01A0013L)
#define STATUS_LOG_POLICY_ALREADY_INSTALLED  ((NTSTATUS)0xC01A0014L)
#define STATUS_LOG_POLICY_NOT_INSTALLED      ((NTSTATUS)0xC01A0015L)
#define STATUS_LOG_POLICY_INVALID            ((NTSTATUS)0xC01A0016L)
#define STATUS_LOG_POLICY_CONFLICT           ((NTSTATUS)0xC01A0017L)
#define STATUS_LOG_PINNED_ARCHIVE_TAIL       ((NTSTATUS)0xC01A0018L)
#define STATUS_LOG_RECORD_NONEXISTENT        ((NTSTATUS)0xC01A0019L)
#define STATUS_LOG_RECORDS_RESERVED_INVALID  ((NTSTATUS)0xC01A001AL)
#define STATUS_LOG_SPACE_RESERVED_INVALID    ((NTSTATUS)0xC01A001BL)
#define STATUS_LOG_TAIL_INVALID              ((NTSTATUS)0xC01A001CL)
#define STATUS_LOG_FULL                      ((NTSTATUS)0xC01A001DL)
#define STATUS_LOG_MULTIPLEXED               ((NTSTATUS)0xC01A001EL)
#define STATUS_LOG_DEDICATED                 ((NTSTATUS)0xC01A001FL)
#define STATUS_LOG_ARCHIVE_NOT_IN_PROGRESS   ((NTSTATUS)0xC01A0020L)
#define STATUS_LOG_ARCHIVE_IN_PROGRESS       ((NTSTATUS)0xC01A0021L)
#define STATUS_LOG_EPHEMERAL                 ((NTSTATUS)0xC01A0022L)
#define STATUS_LOG_NOT_ENOUGH_CONTAINERS     ((NTSTATUS)0xC01A0023L)
#define STATUS_LOG_CLIENT_ALREADY_REGISTERED ((NTSTATUS)0xC01A0024L)
#define STATUS_LOG_CLIENT_NOT_REGISTERED     ((NTSTATUS)0xC01A0025L)
#define STATUS_LOG_FULL_HANDLER_IN_PROGRESS  ((NTSTATUS)0xC01A0026L)
#define STATUS_LOG_CONTAINER_READ_FAILED     ((NTSTATUS)0xC01A0027L)
#define STATUS_LOG_CONTAINER_WRITE_FAILED    ((NTSTATUS)0xC01A0028L)
#define STATUS_LOG_CONTAINER_OPEN_FAILED     ((NTSTATUS)0xC01A0029L)
#define STATUS_LOG_CONTAINER_STATE_INVALID   ((NTSTATUS)0xC01A002AL)
#define STATUS_LOG_STATE_INVALID             ((NTSTATUS)0xC01A002BL)
#define STATUS_LOG_PINNED                    ((NTSTATUS)0xC01A002CL)
#define STATUS_LOG_METADATA_FLUSH_FAILED     ((NTSTATUS)0xC01A002DL)
#define STATUS_LOG_INCONSISTENT_SECURITY     ((NTSTATUS)0xC01A002EL)
#define STATUS_LOG_APPENDED_FLUSH_FAILED     ((NTSTATUS)0xC01A002FL)
#define STATUS_LOG_PINNED_RESERVATION        ((NTSTATUS)0xC01A0030L)
#endif


typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile,
} TYPE_OF_OPEN;

// io/ntfsinit.cpp

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
// io/fastio
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

// io/fsctrl
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

// io/create.cpp
_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCreate(_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

// io/close.cpp
_Function_class_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdClose (_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

// io/read.cpp
_Function_class_(IRP_MJ_READ)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdRead(_In_    PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

// io/write.cpp
_Function_class_(IRP_MJ_WRITE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdWrite (_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

// io/fileinfo.cpp

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
// io/ea.cpp
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

// io/vol.cpp
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
// io/pnp.cpp
_Function_class_(IRP_MJ_PNP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdPnp(_In_    PDEVICE_OBJECT VolumeDeviceObject,
           _Inout_ PIRP Irp);

// io/ntblockio.cpp
NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceToRead,
         _In_    ULONGLONG Offset,
         _In_    ULONG Length,
         _Inout_ PUCHAR Buffer);

NTSTATUS
WriteDisk(_In_    PDEVICE_OBJECT DeviceToWrite,
          _In_    ULONGLONG Offset,
          _In_    ULONG Length,
          _In_    PUCHAR Buffer);

NTSTATUS
DeviceIoControl(_In_    PDEVICE_OBJECT DeviceObject,
                _In_    ULONG ControlCode,
                _In_    PVOID InputBuffer,
                _In_    ULONG InputBufferSize,
                _Inout_ PVOID OutputBuffer,
                _Inout_ PULONG OutputBufferSize,
                _In_    BOOLEAN Override);

// io/registry.cpp
HANDLE
OpenRegistryKey();

#define CloseRegistryKey(Key) ZwClose(Key)

INT
QueryDwordRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ INT Default = 0);

INT
QueryDwordRegistryValue(_In_ PWCHAR Name,
                        _In_ INT Default = 0);

BOOLEAN
QueryBooleanRegistryValue(_In_ HANDLE RegistryKey,
                          _In_ PWCHAR Name,
                          _In_ BOOLEAN Default = FALSE);
BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name,
                          _In_ BOOLEAN Default = FALSE);

NTSTATUS
SetDwordRegistryValue(_In_ HANDLE RegistryKey,
                      _In_ PWCHAR Name,
                      _In_ INT Data);

NTSTATUS
SetBooleanRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ BOOLEAN Data);

VOID
GetGlobalSettingsFromRegistry();

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);

#include "filerecord/attributes/attributes.h"
#include "io/ctxblks.h"
#include "ntfsvol/ntfsvol.h"
#include "filerecord/filerecord.h"
#include "btree/btree.h"
#include "btree/directory/directory.h"
#include "mft/mft.h"
#include "lfs/logfile.h"
#include "lfs/usnjrnl.h"
#include "lfs/lfs.h"
#include <ntstrsafe.h>

// Global variables
extern BOOLEAN gAllowExtChar8dot3;
extern BOOLEAN gShowMetadataFiles;
extern BOOLEAN gShowVersionInfo;
extern BOOLEAN gBugCheckOnCorrupt;
extern BOOLEAN gDisable8dot3NameCreation;
extern INT gDisableLastAccessUpdate;
extern BOOLEAN gDisableLfsDowngrade;
extern BOOLEAN gDisableLfsUpgrade;
extern INT gMftZoneReservation;

#include <debug.h>

#ifdef NTFS_DEBUG

#define PrintFlag(Item, Flag, FlagName) if(Item & Flag) \
DbgPrint("    %s\n", FlagName); \
/* Debug print functions. REMOVE WHEN DONE. */

static inline void PrintUpCaseTable(PUCHAR UpCaseData,
                                    ULONG Length)
{
    DbgPrint("Offset | Value\n");
    for (int i = 0; i < Length; i += 2)
    {
        DbgPrint("0x%2X   | %C\n", i, ((WCHAR)(UpCaseData[i])));
    }
}

static inline void PrintAttrDefTable(PFileRecord AttrDef)
{
    PAttrDefEntry TableEntry;
    ULONG AttrDefEntryIndex, AttrDefDataSize, MaxIndex;
    PUCHAR Buffer;
    PAttribute DataAttr;

    DataAttr = AttrDef->GetAttribute(TypeData, NULL);
    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDef->CopyData(DataAttr,
                      Buffer,
                      &AttrDefDataSize,
                      0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    AttrDefEntryIndex = 0;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;

    DbgPrint(" Type  | Name                       | Flags | Min  | Max  \n");
    DbgPrint("==========================================================\n");

    for (int i = 0; i < MaxIndex; i++)
    {
        DbgPrint(" 0x%03X | %-26S | 0x%02X  | 0x%02X | 0x%X\n",
                 TableEntry->AttributeType,
                 TableEntry->Label,
                 TableEntry->Flags,
                 TableEntry->MinimumSize,
                 TableEntry->MaximumSize);

        // Move onto the next element
        TableEntry++;
    }

    delete Buffer;
}

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
    DbgPrint("File name:          \"%wZ\"\n", &FileCB->FileName);

    if (FileCB->FileDir)
        DbgPrint("Directory:          TRUE\n");
    else
        DbgPrint("Directory:          FALSE\n");
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
