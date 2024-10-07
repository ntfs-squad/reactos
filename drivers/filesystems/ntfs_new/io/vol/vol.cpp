/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new volume managment
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

/* INCLUDES *****************************************************************/
#include "vol.h"

/* GLOBALS *****************************************************************/
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdQueryVolumeInformation)
#pragma alloc_text(PAGE, NtfsFsdSetVolumeInformation)
#pragma alloc_text(PAGE, NtfsMountVolume)
#endif

/* FUNCTIONS ****************************************************************/
_Function_class_(IRP_MJ_QUERY_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                              _Inout_ PIRP Irp)
{
    /* Overview:
     * Returns file system information.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-query-volume-information
     */
    PIO_STACK_LOCATION IoStack;
    FS_INFORMATION_CLASS FSInfoRequest;
    PVolumeContextBlock VolCB;
    NTSTATUS Status;
    PVOID SystemBuffer;
    ULONG BufferLength;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    FSInfoRequest = IoStack->Parameters.QueryVolume.FsInformationClass;
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IoStack->Parameters.QueryFile.Length;

    switch (FSInfoRequest)
    {
        case FileFsVolumeInformation:
            Status = NtfsGetVolumeInformation(VolumeDeviceObject,
                                              (PFILE_FS_VOLUME_INFORMATION)SystemBuffer,
                                              &BufferLength);
            break;
        case FileFsSizeInformation:
            Status = NtfsGetSizeInfo(VolumeDeviceObject,
                                     (PFILE_FS_SIZE_INFORMATION)SystemBuffer,
                                     &BufferLength);
            break;
        case FileFsAttributeInformation:
            Status = NtfsGetAttributeInfo((PFILE_FS_ATTRIBUTE_INFORMATION)SystemBuffer,
                                          &BufferLength);
            break;
        case FileFsControlInformation:
        case FileFsDeviceInformation:
        case FileFsDriverPathInformation:
        case FileFsFullSizeInformation:
        case FileFsObjectIdInformation:
        /* Used in Windows 7+
         * case FileFsSectorSizeInformation:
         */
        default:
            DPRINT1("Unhandled File System Information Request %d!\n", FSInfoRequest);
            Status = STATUS_NOT_IMPLEMENTED;
            break;
    }

    if (NT_SUCCESS(Status))
        Irp->IoStatus.Information =
            IoStack->Parameters.QueryFile.Length - BufferLength;
    else
        Irp->IoStatus.Information = 0;

    return Status;
}

_Function_class_(IRP_MJ_SET_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                            _Inout_ PIRP Irp)
{
    /* Overview:
     * Handles requests to change file system information.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-set-volume-information
     */

    PIO_STACK_LOCATION IoStack;
    FS_INFORMATION_CLASS FSInfoRequest;
    PVolumeContextBlock VolCB;
    NTSTATUS Status;
    PVOID SystemBuffer;
    ULONG BufferLength;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    FSInfoRequest = IoStack->Parameters.QueryVolume.FsInformationClass;
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IoStack->Parameters.QueryFile.Length;

    switch (FSInfoRequest)
    {
        case FileFsLabelInformation:
            Status = NtfsSetVolumeLabel(VolumeDeviceObject,
                                        (PFILE_FS_LABEL_INFORMATION)SystemBuffer,
                                        &BufferLength);
            break;
        case FileFsControlInformation:
        case FileFsObjectIdInformation:
        default:
            DPRINT1("Unhandled File System Set Information Request %d!\n", FSInfoRequest);
            Status = STATUS_NOT_IMPLEMENTED;
            break;
    }

    if (NT_SUCCESS(Status))
        Irp->IoStatus.Information =
            IoStack->Parameters.QueryFile.Length - BufferLength;
    else
        Irp->IoStatus.Information = 0;

    return Status;
}

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject)
{
    PDEVICE_OBJECT FSDeviceObject;
    NTFSVolume* Volume;
    NTSTATUS Status;
    PVolumeContextBlock VolCB;
    LARGE_INTEGER FilesystemSize;

    /* The function here returns, but it's not an error.
     * We're a boot driver, NT will try every possible filesystem.
     */
    Volume = new(PagedPool) NTFSVolume();
    Status = Volume->LoadNTFSDevice(TargetDeviceObject);

    DPRINT1("LoadNTFSDevice() returned %lx\n", Status);
    if (Status != STATUS_SUCCESS)
        return Status;

    // Currently used for debugging output.
    Volume->RunSanityChecks();

    // Create file system device object.
    Status = IoCreateDevice(NtfsDriverObject,
                            sizeof(VolumeContextBlock),
                            NULL,
                            FILE_DEVICE_DISK_FILE_SYSTEM,
                            0,
                            FALSE,
                            &FSDeviceObject);

    if (!NT_SUCCESS(Status))
        __debugbreak();

    // Tell IO Manager to directly transfer data to FSDeviceObject.
    FSDeviceObject->Flags |= DO_BUFFERED_IO; // DO_DIRECT_IO;

    // Initialize Volume Context Block VolCB.
    VolCB = (PVolumeContextBlock)FSDeviceObject->DeviceExtension;
    RtlZeroMemory(VolCB, sizeof(VolumeContextBlock));
    FSDeviceObject->Vpb = TargetDeviceObject->Vpb;

    // Give VolCB access to Ntfs Partition object
    VolCB->Volume = Volume;

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

    // Set file system size information.
    FilesystemSize.QuadPart = Volume->ClustersInVolume * Volume->SectorsPerCluster * Volume->BytesPerSector;

    // Get serial number.
    FSDeviceObject->Vpb->SerialNumber = Volume->SerialNumber;

    // Get volume label.
    Status = Volume->GetVolumeLabel(FSDeviceObject->Vpb->VolumeLabel,
                                      &FSDeviceObject->Vpb->VolumeLabelLength);

    // Mount volume.
    FsRtlNotifyVolumeEvent(VolCB->StreamFileObject, FSRTL_VOLUME_MOUNT);

    Status = STATUS_SUCCESS;
    return Status;
}