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

typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile,
} TYPE_OF_OPEN;

#include "contextblocks.h"

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
NtfsFsdCleanup (_In_ PDEVICE_OBJECT VolumeDeviceObject,
                _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_LOCK_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdLockControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                   _Inout_ PIRP Irp)

_Function_class_(IRP_MJ_DEVICE_CONTROL)
_Function_class_(DRIVER_DISPATCH);
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDeviceControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp);


_Function_class_(IRP_MJ_SHUTDOWN)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdShutdown (_In_ PDEVICE_OBJECT VolumeDeviceObject,
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
NtfsFsdFileSystemControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                         _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_FLUSH_BUFFERS)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFlushBuffers (_In_ PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp);

/* Create.cpp */
_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCreate(_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

/* close.cpp  */
_Function_class_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdClose (_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

/* read.cpp */
_Function_class_(IRP_MJ_READ)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdRead(_In_ PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

/* write.cpp */
_Function_class_(IRP_MJ_WRITE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdWrite (_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

/* fileinfo.cpp */

_Function_class_(IRP_MJ_QUERY_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_DIRECTORY_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDirectoryControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SET_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp);
/* ea.cpp */
_Function_class_(IRP_MJ_QUERY_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryEa(_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SET_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetEa(_In_ PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

/* vol.cpp */
_Function_class_(IRP_MJ_QUERY_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
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
NtfsFsdSetVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                            _Inout_ PIRP Irp);
/* pnp.cpp */
_Function_class_(IRP_MJ_PNP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdPnp(_In_ PDEVICE_OBJECT VolumeDeviceObject,
           _Inout_ PIRP Irp);

/* ntblockio.cpp*/
NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceBeingRead,
         _In_    LONGLONG StartingOffset,
         _In_    ULONG AmountOfSectors,
         _In_    ULONG SectorSize,
         _Inout_ PUCHAR Buffer,
         _In_    BOOLEAN Override);

NTSTATUS
ReadBlock(_In_    PDEVICE_OBJECT DeviceObject,
          _In_    ULONG DiskSector,
          _In_    ULONG SectorCount,
          _In_    ULONG SectorSize,
          _Inout_ PUCHAR Buffer,
          _In_    BOOLEAN Override);

NTSTATUS
WriteDisk(_In_    PDEVICE_OBJECT DeviceBeingRead,
          _In_    LONGLONG StartingOffset,
          _In_    ULONG AmountOfBytes,
          _In_    PUCHAR BufferToWrite);

NTSTATUS
WriteBlock(_In_    PDEVICE_OBJECT DeviceObject,
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

#include "../attributes.h"
#include "../ntfspartition.h"
#include "../filerecord.h"

#include <debug.h>

/* Debug print functions. REMOVE WHEN DONE. */
static inline void PrintFileRecordHeader(FileRecordHeader* FRH)
{
    UCHAR Signature[5];

    RtlZeroMemory(Signature, 5);
    RtlCopyMemory(Signature, FRH->TypeID, 4);

    DPRINT1("Signature: %s\n", Signature);
    DPRINT1("MFT Record Number: %ld\n", FRH->MFTRecordNumber);
}

static inline void PrintAttributeHeader(IAttribute* Attr)
{
    DPRINT1("Attribute Type:   0x%X\n", Attr->AttributeType);
    DPRINT1("Length:           %ld\n", Attr->Length);
    DPRINT1("Nonresident Flag: %ld\n", Attr->NonResidentFlag);
    DPRINT1("Name Length:      %ld\n", Attr->NameLength);
    DPRINT1("Name Offset:      %ld\n", Attr->NameOffset);
    DPRINT1("Flags:            0x%X\n", Attr->Flags);
    DPRINT1("Attribute ID:     %ld\n", Attr->AttributeID);
}

static inline void PrintResidentAttributeHeader(ResidentAttribute* Attr)
{
    PrintAttributeHeader(Attr);

    DPRINT1("Attribute Length: %ld\n", Attr->AttributeLength);
    DPRINT1("Attribute Offset: 0x%X\n", Attr->AttributeOffset);
    DPRINT1("IndexedFlag:      %ld\n", Attr->IndexedFlag);
}

static inline void PrintNonResidentAttributeHeader(NonResidentAttribute* Attr)
{
    PrintAttributeHeader(Attr);

    DPRINT1("First VCN:                %ld\n", Attr->FirstVCN);
    DPRINT1("Last VCN:                 %ld\n", Attr->LastVCN);
    DPRINT1("Data Run Offset:          %ld\n", Attr->DataRunsOffset);
    DPRINT1("Compression Unit Size:    %ld\n", Attr->CompressionUnitSize);
    DPRINT1("Allocated Attribute Size: %ld\n", Attr->AllocatedAttributeSize);
    DPRINT1("Actual Attribute Size:    %ld\n", Attr->ActualAttributeSize);
    DPRINT1("Initialized Data Size:    %ld\n", Attr->InitalizedDataSize);
}

static inline void PrintFilenameAttrHeader(FileNameEx* Attr)
{
    DPRINT1("FileCreation: %ld\n", Attr->FileCreation);
    DPRINT1("FileChanged: %ld\n", Attr->FileChanged);
    DPRINT1("MftChanged: %ld\n", Attr->MftChanged);
    DPRINT1("FileRead: %ld\n", Attr->FileRead);
    DPRINT1("AllocatedSize: %ld\n", Attr->AllocatedSize);
    DPRINT1("RealSize: %ld\n", Attr->RealSize);
    DPRINT1("FilenameChars: %ld\n", Attr->FilenameChars);
}

static inline void PrintNTFSBootSector(BootSector* PartBootSector)
{
    DPRINT1("OEM ID            %s\n", PartBootSector->OEM_ID);
    DPRINT1("Bytes per sector  %ld\n", PartBootSector->BytesPerSector);
    DPRINT1("Sectors/cluster   %ld\n", PartBootSector->SectorsPerCluster);
    DPRINT1("Sectors per track %ld\n", PartBootSector->SectorsPerTrack);
    DPRINT1("Number of heads   %ld\n", PartBootSector->NumberOfHeads);
    DPRINT1("Sectors in volume %ld\n", PartBootSector->SectorsInVolume);
    DPRINT1("LCN for $MFT      %ld\n", PartBootSector->MFTLCN);
    DPRINT1("LCN for $MFT_MIRR %ld\n", PartBootSector->MFTMirrLCN);
    DPRINT1("Clusters/MFT Rec  %d\n", PartBootSector->ClustersPerFileRecord);
    DPRINT1("Clusters/IndexRec %d\n", PartBootSector->ClustersPerIndexRecord);
    DPRINT1("Serial number     0x%X\n", PartBootSector->SerialNumber);
};

static inline void PrintFileContextBlock(PFileContextBlock FileCB)
{
    DPRINT1("Path Name:   \"%S\"\n", FileCB->PathName);
    DPRINT1("Object Name: \"%S\"\n", FileCB->ObjectName);
};

#endif // _NTFSPROCS_


