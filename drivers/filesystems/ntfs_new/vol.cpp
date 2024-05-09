/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new volume managment
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdQueryVolumeInformation)
#pragma alloc_text(PAGE, NtfsFsdSetVolumeInformation)
#pragma alloc_text(PAGE, NtfsMountVolume)
#endif
extern NPAGED_LOOKASIDE_LIST FcbLookasideList;
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

NTSTATUS
AreWeNtfs(PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    ULONG ClusterSize, Size, k;
    PBOOT_SECTOR BootSector;
    NTSTATUS Status;

    DPRINT("NtfsHasFileSystem() called\n");

    Size = sizeof(DISK_GEOMETRY);
    Status = DeviceIoControl(DeviceToMount,
                            IOCTL_DISK_GET_DRIVE_GEOMETRY,
                            NULL,
                            0,
                            &DiskGeometry,
                            &Size,
                            TRUE);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("NtfsDeviceIoControl() failed (Status %lx)\n", Status);
        return Status;
    }

    if (DiskGeometry.BytesPerSector > 512)
        Status = STATUS_UNRECOGNIZED_VOLUME;

    BootSector = (PBOOT_SECTOR)ExAllocatePoolWithTag(NonPagedPool,
                                       DiskGeometry.BytesPerSector,
                                       TAG_NTFS);
    if (BootSector == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ReadBlock(DeviceToMount,
              0,
              1,
              DiskGeometry.BytesPerSector,
              (PUCHAR)BootSector,
              TRUE);
    if (!NT_SUCCESS(Status))
    {
        goto ByeBye;
    }

    /*
     * Check values of different fields. If those fields have not expected
     * values, we fail, to avoid mounting partitions that Windows won't mount.
     */

    /* OEMID: this field must be NTFS */
    if (RtlCompareMemory(BootSector->OEMID, "NTFS    ", 8) != 8)
    {
        DPRINT1("Failed with NTFS-identifier: [%.8s]\n", BootSector->OEMID);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto ByeBye;
    }

    /* Unused0: this field must be COMPLETELY null */
    for (k = 0; k < 7; k++)
    {
        if (BootSector->BPB.Unused0[k] != 0)
        {
            DPRINT1("Failed in field Unused0: [%.7s]\n", BootSector->BPB.Unused0);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto ByeBye;
        }
    }

    /* Unused3: this field must be COMPLETELY null */
    for (k = 0; k < 4; k++)
    {
        if (BootSector->BPB.Unused3[k] != 0)
        {
            DPRINT1("Failed in field Unused3: [%.4s]\n", BootSector->BPB.Unused3);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto ByeBye;
        }
    }

    /* Check cluster size */
    ClusterSize = BootSector->BPB.BytesPerSector * BootSector->BPB.SectorsPerCluster;
    if (ClusterSize != 512 && ClusterSize != 1024 &&
        ClusterSize != 2048 && ClusterSize != 4096 &&
        ClusterSize != 8192 && ClusterSize != 16384 &&
        ClusterSize != 32768 && ClusterSize != 65536)
    {
        DPRINT1("Cluster size failed: %hu, %hu, %hu\n",
                BootSector->BPB.BytesPerSector,
                BootSector->BPB.SectorsPerCluster,
                ClusterSize);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto ByeBye;
    }

ByeBye:
    ExFreePool(BootSector);

    return Status;
}

//TODO:
extern PDRIVER_OBJECT NtfsDriverObject;


PNTFS_FCB
NtfsCreateFCB(PCWSTR FileName,
              PCWSTR Stream,
              PNTFS_VCB Vcb)
{
    PNTFS_FCB Fcb;
    Fcb = (PNTFS_FCB)ExAllocateFromNPagedLookasideList(&FcbLookasideList);
    if (Fcb == NULL)
    {
        return NULL;
    }

    RtlZeroMemory(Fcb, sizeof(NTFS_FCB));

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


typedef struct
{
    LIST_ENTRY     NextCCB;
    PFILE_OBJECT   PtrFileObject;
    LARGE_INTEGER  CurrentByteOffset;
    /* for DirectoryControl */
    ULONG Entry;
    /* for DirectoryControl */
    PWCHAR DirectorySearchPattern;
    ULONG LastCluster;
    ULONG LastOffset;
} NTFS_CCB, *PNTFS_CCB;

PDEVICE_OBJECT StorageDevice;

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject)
{
    PNTFS_VCB Vcb = NULL;
    PNTFS_FCB Fcb = NULL;
    PNTFS_CCB Ccb = NULL;
    PDEVICE_OBJECT NewDeviceObject = NULL;
    NTSTATUS Status;
   /* The function here returns, but it's not an error. we're a boot driver
     * NT will try every possible filesystem dr
     */
    Status = AreWeNtfs(TargetDeviceObject);
    DPRINT1("AreWeNtfs() returned %lx\n", Status);
    if (Status != STATUS_SUCCESS)
        return Status;

    NtfsPartition* LocNtfsPart;
    LocNtfsPart = new(PagedPool) NtfsPartition(TargetDeviceObject);
    LocNtfsPart->RunSanityChecks();

    Status = IoCreateDevice(NtfsDriverObject,
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_DISK_FILE_SYSTEM,
                            0,
                            FALSE,
                            &NewDeviceObject);
    if (!NT_SUCCESS(Status))
        __debugbreak();

    NewDeviceObject->Flags |= DO_DIRECT_IO;

    Vcb = (PNTFS_VCB)NewDeviceObject->DeviceExtension;
    RtlZeroMemory(Vcb, sizeof(NTFS_VCB));
    NewDeviceObject->Vpb = TargetDeviceObject->Vpb;

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

    Fcb = NtfsCreateFCB(NULL, NULL, Vcb);
    if (Fcb == NULL)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        __debugbreak();
    }

    Ccb = ( PNTFS_CCB )ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(NTFS_CCB),
                                TAG_NTFS);
    if (Ccb == NULL)
    {
        Status =  STATUS_INSUFFICIENT_RESOURCES;
        __debugbreak();
    }

    RtlZeroMemory(Ccb, sizeof(NTFS_CCB));


    Vcb->StreamFileObject->FsContext = Fcb;
    Vcb->StreamFileObject->FsContext2 = Ccb;
    Vcb->StreamFileObject->SectionObjectPointer = &Fcb->SectionObjectPointers;
    Vcb->StreamFileObject->PrivateCacheMap = NULL;
    Vcb->StreamFileObject->Vpb = Vcb->Vpb;
    Ccb->PtrFileObject = Vcb->StreamFileObject;
    Fcb->FileObject = Vcb->StreamFileObject;
    Fcb->Vcb = (PDEVICE_EXTENSION)Vcb->StorageDevice;
#define FCB_IS_VOLUME_STREAM    0x0002
    Fcb->Flags = FCB_IS_VOLUME_STREAM;


/* ACTUAL CLASS USAGE HERE ->*/
    Fcb->RFCB.FileSize.QuadPart = LocNtfsPart->VCB->SectorsInVolume * LocNtfsPart->VCB->BytesPerSector;
    Fcb->RFCB.ValidDataLength.QuadPart = LocNtfsPart->VCB->SectorsInVolume * LocNtfsPart->VCB->BytesPerSector;
    Fcb->RFCB.AllocationSize.QuadPart = LocNtfsPart->VCB->SectorsInVolume * LocNtfsPart->VCB->BytesPerSector;
    
    ExInitializeResourceLite(&Vcb->DirResource);

    KeInitializeSpinLock(&Vcb->FcbListLock);
    /* Get serial number */
    NewDeviceObject->Vpb->SerialNumber = LocNtfsPart->VCB->SerialNumber;
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