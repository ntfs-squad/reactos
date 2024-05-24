/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new volume managment
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"
#include "contextblocks.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdQueryVolumeInformation)
#pragma alloc_text(PAGE, NtfsFsdSetVolumeInformation)
#pragma alloc_text(PAGE, NtfsMountVolume)
#endif
extern NPAGED_LOOKASIDE_LIST FileCBLookasideList;
#define FCB_IS_VOLUME_STREAM    0x0002

//TODO:
extern PDRIVER_OBJECT NtfsDriverObject;

/* FUNCTIONS ****************************************************************/

_Function_class_(IRP_MJ_QUERY_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                              _Inout_ PIRP Irp)
{
    DPRINT1("TODO: NtfsFsdQueryVolumeInformation() called\n");
    Irp->IoStatus.Information = 0;
    IoSkipCurrentIrpStackLocation(Irp);

    return 0;
}

_Function_class_(IRP_MJ_SET_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                            _Inout_ PIRP Irp)
{

    DPRINT("NtfsFsdSetVolumeInformation() called\n");
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;

    return STATUS_NOT_SUPPORTED;
}

PDEVICE_OBJECT StorageDevice;

// TODO: Do we need this? I don't think this belongs here.
PFileContextBlock
NtfsCreateFileCB(PCWSTR FileName,
                 PCWSTR Stream,
                 PVolumeContextBlock VolCB)
{
    PFileContextBlock FileCB;

    // FileCB = (PFileContextBlock)ExAllocateFromNPagedLookasideList(&FileCBLookasideList);
    FileCB = new(NonPagedPool) FileContextBlock();
    if (FileCB == NULL)
    {
        return NULL;
    }

    RtlZeroMemory(FileCB, sizeof(FileContextBlock));

    FileCB->VolCB = VolCB;

    if (FileName)
    {
        wcscpy(FileCB->PathName, FileName);
        if (wcsrchr(FileCB->PathName, '\\') != 0)
        {
            FileCB->ObjectName = wcsrchr(FileCB->PathName, '\\');
        }
        else
        {
            FileCB->ObjectName = FileCB->PathName;
        }
    }

    if (Stream)
    {
        wcscpy(FileCB->Stream, Stream);
    }
    else
    {
        FileCB->Stream[0] = UNICODE_NULL;
    }

    ExInitializeResourceLite(&FileCB->MainResource);

    FileCB->RFCB.Resource = &(FileCB->MainResource);

    return FileCB;
}

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject)
{
    PDEVICE_OBJECT FSDeviceObject;
    NtfsPartition* NtfsPart;
    NTSTATUS Status;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    // PClusterContextBlock ClusCB;  TODO: Remove?

    /* The function here returns, but it's not an error.
     * We're a boot driver, NT will try every possible filesystem.
     */
    NtfsPart = new(PagedPool) NtfsPartition();
    Status = NtfsPart->LoadNtfsDevice(TargetDeviceObject);

    DPRINT1("LoadNtfsDevice() returned %lx\n", Status);
    if (Status != STATUS_SUCCESS)
        return Status;

    NtfsPart->RunSanityChecks();

    Status = IoCreateDevice(NtfsDriverObject,
                            sizeof(VolumeContextBlock),
                            NULL,
                            FILE_DEVICE_DISK_FILE_SYSTEM,
                            0,
                            FALSE,
                            &FSDeviceObject);

    if (!NT_SUCCESS(Status))
        __debugbreak();
    DPRINT1("Io device created!\n");

    // Tell IO Manager to directly transfer data to FSDeviceObject.
    FSDeviceObject->Flags |= DO_DIRECT_IO;

    // Initialize Volume Context Block VolCB.
    VolCB = (PVolumeContextBlock)FSDeviceObject->DeviceExtension;
    RtlZeroMemory(VolCB, sizeof(VolumeContextBlock));
    FSDeviceObject->Vpb = TargetDeviceObject->Vpb;

    DPRINT1("VolCB created!\n");

    // Set up storage device in VolCB.
    VolCB->StorageDevice = TargetDeviceObject;
    VolCB->StorageDevice->Vpb->DeviceObject = FSDeviceObject;
    VolCB->StorageDevice->Vpb->RealDevice = VolCB->StorageDevice;
    VolCB->StorageDevice->Vpb->Flags |= VPB_MOUNTED;
    FSDeviceObject->StackSize = VolCB->StorageDevice->StackSize + 1;

    // Tell IO manager we are done initializing.
    FSDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    // Create file stream object.
    VolCB->StreamFileObject = IoCreateStreamFileObject(NULL,
                                                       VolCB->StorageDevice);
    StorageDevice = VolCB->StorageDevice; //HACKHACKHACK
    InitializeListHead(&VolCB->FileCBListHead);

    DPRINT1("Created Stream File Object!\n");

    FileCB = NtfsCreateFileCB(NULL, NULL, VolCB);
    if (FileCB == NULL)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        __debugbreak();
    }

    // TODO: I don't think we need this. Remove?
    /*ClusCB = (PClusterContextBlock)ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(ClusterContextBlock),
                                TAG_NTFS);
    if (ClusCB == NULL)
    {
        Status =  STATUS_INSUFFICIENT_RESOURCES;
        __debugbreak();
    }

    RtlZeroMemory(ClusCB, sizeof(ClusterContextBlock));
    DPRINT1("ClusCB created!\n");

    VolCB->StreamFileObject->FsContext2 = ClusCB;
    ClusCB->PtrFileObject = VolCB->StreamFileObject;
    */

    VolCB->StreamFileObject->FsContext = FileCB;
    VolCB->StreamFileObject->SectionObjectPointer = &FileCB->SectionObjectPointers;
    VolCB->StreamFileObject->PrivateCacheMap = NULL;
    VolCB->StreamFileObject->Vpb = VolCB->Vpb;
    FileCB->FileObject = VolCB->StreamFileObject;
    FileCB->VolCB = (PVolumeContextBlock)VolCB->StorageDevice;
    FileCB->Flags = FCB_IS_VOLUME_STREAM;
    DPRINT1("FileContextBlock created!\n");

    // Set file size information.
    FileCB->RFCB.FileSize.QuadPart = NtfsPart->SectorsInVolume * NtfsPart->BytesPerSector;
    FileCB->RFCB.ValidDataLength.QuadPart = NtfsPart->SectorsInVolume * NtfsPart->BytesPerSector;
    FileCB->RFCB.AllocationSize.QuadPart = NtfsPart->SectorsInVolume * NtfsPart->BytesPerSector;

    DPRINT1("FileContextBlock updated!\n");

    ExInitializeResourceLite(&VolCB->DirResource);
    KeInitializeSpinLock(&VolCB->FileCBListLock);

    // Get serial number
    FSDeviceObject->Vpb->SerialNumber = NtfsPart->SerialNumber;
    __debugbreak();

    // Get Volume Label
    Status = NtfsPart->GetVolumeLabel(FSDeviceObject->Vpb->VolumeLabel,
                                      FSDeviceObject->Vpb->VolumeLabelLength);

    DPRINT1("Volume Label updated!\n");
    DPRINT1("Label: \"%S\", Length: %ld\n",
            FSDeviceObject->Vpb->VolumeLabel,
            FSDeviceObject->Vpb->VolumeLabelLength);

    // Mount Volume
    FsRtlNotifyVolumeEvent(VolCB->StreamFileObject, FSRTL_VOLUME_MOUNT);

    Status = STATUS_SUCCESS;
    return Status;
}