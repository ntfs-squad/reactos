/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new volume management
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

 #include "ntfspch.h"

 extern NPAGED_LOOKASIDE_LIST FileCBLookasideList;
 //TODO:

 static
 NTSTATUS
 NtfsGetVolumeInformation(PDEVICE_OBJECT DeviceObject,
                          PFILE_FS_VOLUME_INFORMATION Buffer,
                          PULONG Length)
 {
     size_t VolumeInfoSize = sizeof(FILE_FS_VOLUME_INFORMATION);
 
     if (*Length < VolumeInfoSize + DeviceObject->Vpb->VolumeLabelLength)
         return STATUS_BUFFER_TOO_SMALL;
 
     Buffer->VolumeSerialNumber = DeviceObject->Vpb->SerialNumber;
     Buffer->VolumeLabelLength = DeviceObject->Vpb->VolumeLabelLength;
     RtlCopyMemory(Buffer->VolumeLabel,
                   DeviceObject->Vpb->VolumeLabel,
                   DeviceObject->Vpb->VolumeLabelLength);
 
     // TODO: Fix this
     Buffer->VolumeCreationTime.QuadPart = 0;
     Buffer->SupportsObjects = FALSE;
 
     // TODO: Investigate. Should we be returning the bytes written instead?
     *Length -= VolumeInfoSize + DeviceObject->Vpb->VolumeLabelLength;
 
     return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 NtfsGetSizeInfo(PDEVICE_OBJECT DeviceObject,
                 PFILE_FS_SIZE_INFORMATION Buffer,
                 PULONG Length)
 {
     PNTFSVolume Volume;
 
     if (*Length < sizeof(FILE_FS_SIZE_INFORMATION))
         return STATUS_BUFFER_OVERFLOW;
 
     Volume = ((PVolumeContextBlock)(DeviceObject->DeviceExtension))->Volume;
 
     if (!Volume)
         return STATUS_INSUFFICIENT_RESOURCES;
 
     Volume->GetFreeClusters(&Buffer->AvailableAllocationUnits);
     Buffer->TotalAllocationUnits.QuadPart = Volume->ClustersInVolume;
     Buffer->SectorsPerAllocationUnit = Volume->SectorsPerCluster;
     Buffer->BytesPerSector = Volume->BytesPerSector;
 
     *Length -= sizeof(FILE_FS_SIZE_INFORMATION);
 
     return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 NtfsGetAttributeInfo(PNTFSVolume Volume,
                      PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
                      PULONG Length)
 {
     NTSTATUS Status;
     size_t BytesToWrite;
     LPCWSTR NTFSVerFormat;
     UNICODE_STRING NTFSVer;
 
     if (gShowVersionInfo)
     {
         // Report "NTFS x.x, Client x.x"
         BytesToWrite = sizeof(FILE_FS_ATTRIBUTE_INFORMATION) + 38;
         if (*Length < BytesToWrite)
             goto fallback;
         Buffer->FileSystemNameLength = 40;
         NTFSVerFormat = L"NTFS %1ld.%1ld, Client %1ld.%1ld";
         RtlInitEmptyUnicodeString(&NTFSVer,
                                   Buffer->FileSystemName,
                                   40);
         Status = RtlUnicodeStringPrintf(&NTFSVer,
                                         NTFSVerFormat,
                                         Volume->NtfsMajorVersion,
                                         Volume->NtfsMinorVersion,
                                         Volume->LFS->ClientMajorVersion,
                                         Volume->LFS->ClientMinorVersion);
         if (!NT_SUCCESS(Status))
             goto fallback;
     }
 
     else
     {
 fallback:
         // Report "NTFS"
         BytesToWrite = sizeof(FILE_FS_ATTRIBUTE_INFORMATION) + 6;
         if (*Length < BytesToWrite)
             return STATUS_BUFFER_TOO_SMALL;
         Buffer->FileSystemNameLength = 8;
         RtlCopyMemory(Buffer->FileSystemName, L"NTFS", 8);
         *Length -= BytesToWrite;
     }
 
     /* For more information on FileSystemAttributes:
      * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-smb/3065351b-0b78-4976-9a5a-11657d8857c7
      *
      * TODO: Add attributes as needed.
      */
     Buffer->FileSystemAttributes = FILE_CASE_PRESERVED_NAMES
                                    | FILE_UNICODE_ON_DISK
                                    | FILE_NAMED_STREAMS;
 
     if (Volume->IsReadOnly)
         Buffer->FileSystemAttributes |= FILE_READ_ONLY_VOLUME;
 
     Buffer->MaximumComponentNameLength = 255;
     *Length -= BytesToWrite;
     return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 NtfsSetVolumeLabel(_In_ PDEVICE_OBJECT DeviceObject,
                    _In_ PFILE_FS_LABEL_INFORMATION NewLabel,
                    _In_ PULONG Length)
 {
     NTSTATUS Status;
     PNTFSVolume Volume;
 
     Volume = ((PVolumeContextBlock)(DeviceObject->DeviceExtension))->Volume;
 
     if (!Volume || !NewLabel)
         return STATUS_INSUFFICIENT_RESOURCES;
 
     Volume->SetVolumeLabel(NewLabel->VolumeLabel, NewLabel->VolumeLabelLength);
 
     // Re-read volume label.
     Status = Volume->GetVolumeLabel(DeviceObject->Vpb->VolumeLabel,
                                     &DeviceObject->Vpb->VolumeLabelLength);
 
     return Status;
 }

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
            Status = NtfsGetAttributeInfo(VolCB->Volume,
                                          (PFILE_FS_ATTRIBUTE_INFORMATION)SystemBuffer,
                                          &BufferLength);
            break;
        case FileFsControlInformation:
        case FileFsDeviceInformation:
        case FileFsDriverPathInformation:
        case FileFsFullSizeInformation:
            DPRINT1("FileFsFullSizeInformation() request not implemented!\n");
            Status = STATUS_NOT_IMPLEMENTED;
            break;
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

    // Do not force buffered or direct I/O at FS level; leave to I/O manager/CC
    
    // Set up FastIo dispatch table for this volume
    FSDeviceObject->DriverObject->FastIoDispatch = &FastIoDispatch;

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