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
extern NPAGED_LOOKASIDE_LIST FcbLookasideList;
#define FCB_IS_VOLUME_STREAM    0x0002
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

//TODO:
extern PDRIVER_OBJECT NtfsDriverObject;

PFileContextBlock
NtfsCreateFCB(PCWSTR FileName,
              PCWSTR Stream,
              PVolumeContextBlock Vcb)
{
    PFileContextBlock Fcb;

    // Fcb = (PFileContextBlock)ExAllocateFromNPagedLookasideList(&FcbLookasideList);
    Fcb = new(NonPagedPool) FileContextBlock();
    if (Fcb == NULL)
    {
        return NULL;
    }

    RtlZeroMemory(Fcb, sizeof(FileContextBlock));

    Fcb->Vcb = Vcb;

    if (FileName)
    {
        wcscpy(Fcb->PathName, FileName);
        if (wcsrchr(Fcb->PathName, '\\') != 0)
        {
            Fcb->ObjectName = wcsrchr(Fcb->PathName, '\\');
        }
        else
        {
            Fcb->ObjectName = Fcb->PathName;
        }
    }

    if (Stream)
    {
        wcscpy(Fcb->Stream, Stream);
    }
    else
    {
        Fcb->Stream[0] = UNICODE_NULL;
    }

    ExInitializeResourceLite(&Fcb->MainResource);

    Fcb->RFCB.Resource = &(Fcb->MainResource);

    return Fcb;
}

PDEVICE_OBJECT StorageDevice;

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject)
{
    PVolumeContextBlock Vcb;
    PFileContextBlock Fcb;
    PClusterContextBlock Ccb;
    PDEVICE_OBJECT NewDeviceObject;
    NtfsPartition* NtfsPart;
    NTSTATUS Status;

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
                            &NewDeviceObject);

    if (!NT_SUCCESS(Status))
        __debugbreak();
    DPRINT1("Io device created!\n");

    NewDeviceObject->Flags |= DO_DIRECT_IO;

    Vcb = (PVolumeContextBlock)NewDeviceObject->DeviceExtension;
    RtlZeroMemory(Vcb, sizeof(VolumeContextBlock));
    NewDeviceObject->Vpb = TargetDeviceObject->Vpb;

    DPRINT1("VolContextBlock created!\n");

    Vcb->StorageDevice = TargetDeviceObject;
    Vcb->StorageDevice->Vpb->DeviceObject = NewDeviceObject;
    Vcb->StorageDevice->Vpb->RealDevice = Vcb->StorageDevice;
    Vcb->StorageDevice->Vpb->Flags |= VPB_MOUNTED;
    NewDeviceObject->StackSize = Vcb->StorageDevice->StackSize + 1;
    NewDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    Vcb->StreamFileObject = IoCreateStreamFileObject(NULL,
                                                     Vcb->StorageDevice);
    StorageDevice = Vcb->StorageDevice; //HACKHACKHACK
    InitializeListHead(&Vcb->FcbListHead);
    DPRINT1("Created Stream File Object!\n");

    Fcb = NtfsCreateFCB(NULL, NULL, Vcb);
    if (Fcb == NULL)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        __debugbreak();
    }

    Ccb = (PClusterContextBlock)ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(ClusterContextBlock),
                                TAG_NTFS);
    if (Ccb == NULL)
    {
        Status =  STATUS_INSUFFICIENT_RESOURCES;
        __debugbreak();
    }

    RtlZeroMemory(Ccb, sizeof(ClusterContextBlock));
    DPRINT1("ClusterContextBlock created!\n");


    Vcb->StreamFileObject->FsContext = Fcb;
    Vcb->StreamFileObject->FsContext2 = Ccb;
    Vcb->StreamFileObject->SectionObjectPointer = &Fcb->SectionObjectPointers;
    Vcb->StreamFileObject->PrivateCacheMap = NULL;
    Vcb->StreamFileObject->Vpb = Vcb->Vpb;
    Ccb->PtrFileObject = Vcb->StreamFileObject;
    Fcb->FileObject = Vcb->StreamFileObject;
    Fcb->Vcb = (PVolumeContextBlock)Vcb->StorageDevice;
    Fcb->Flags = FCB_IS_VOLUME_STREAM;
    DPRINT1("FileContextBlock created!\n");


/* ACTUAL CLASS USAGE HERE ->*/
    Fcb->RFCB.FileSize.QuadPart = NtfsPart->SectorsInVolume * NtfsPart->BytesPerSector;
    Fcb->RFCB.ValidDataLength.QuadPart = NtfsPart->SectorsInVolume * NtfsPart->BytesPerSector;
    Fcb->RFCB.AllocationSize.QuadPart = NtfsPart->SectorsInVolume * NtfsPart->BytesPerSector;
    DPRINT1("FileContextBlock updated!\n");

    ExInitializeResourceLite(&Vcb->DirResource);

    KeInitializeSpinLock(&Vcb->FcbListLock);
    /* Get serial number */
    NewDeviceObject->Vpb->SerialNumber = NtfsPart->SerialNumber;
    __debugbreak();
    /* MAde up */
    wchar_t* BullshitLabel = L"Hello World";
    NewDeviceObject->Vpb->VolumeLabelLength = sizeof(WCHAR) * 11;
    RtlCopyMemory(NewDeviceObject->Vpb->VolumeLabel,
                  BullshitLabel,
                  NewDeviceObject->Vpb->VolumeLabelLength);

    FsRtlNotifyVolumeEvent(Vcb->StreamFileObject, FSRTL_VOLUME_MOUNT);

    Status = STATUS_SUCCESS;
    return Status;
}