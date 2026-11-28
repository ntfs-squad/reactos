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
    PNtfsVolume DiskVolume;

    if (*Length < sizeof(FILE_FS_SIZE_INFORMATION))
        return STATUS_BUFFER_OVERFLOW;

    DiskVolume = ((PVolumeContextBlock)(DeviceObject->DeviceExtension))->DiskVolume;

    if (!DiskVolume)
        return STATUS_INSUFFICIENT_RESOURCES;

    NtfsVolumeGetFreeClusters(DiskVolume, &Buffer->AvailableAllocationUnits);
    Buffer->TotalAllocationUnits.QuadPart = NtfsVolumeGetClustersInVolume(DiskVolume);
    Buffer->SectorsPerAllocationUnit = NtfsVolumeGetSectorsPerCluster(DiskVolume);
    Buffer->BytesPerSector = NtfsVolumeGetBytesPerSector(DiskVolume);

    *Length -= sizeof(FILE_FS_SIZE_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
NtfsGetAttributeInfo(PNtfsVolume DiskVolume,
                     PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
                     PULONG Length)
{
    NTSTATUS Status;
    size_t BytesToWrite;

    if (!gShowVersionInfo)
        goto Fallback;

    UNICODE_STRING NTFSVer;
    NtfsVersion Version;
    NtfsVolumeGetVersion(DiskVolume, &Version);

    // ShowVersionInfo doesn't account for versions with 2+ digits
    if (Version.MajorVersion > 9 || Version.MinorVersion > 9)
        goto Fallback;

    if (NtfsVolumeIsReadOnly(DiskVolume))
    {
        // Report "NTFS x.x"
        const UNICODE_STRING Format = RTL_CONSTANT_STRING(L"NTFS %c.%c");
        const ULONG FsNameSize = Format.Length - (2 * sizeof(WCHAR));
        BytesToWrite = FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + FsNameSize;
        if (*Length < BytesToWrite)
            goto Fallback;
        RtlInitEmptyUnicodeString(&NTFSVer,
                                  Buffer->FileSystemName,
                                  FsNameSize);
        Status = RtlUnicodeStringPrintf(&NTFSVer,
                                        Format.Buffer,
                                        (WCHAR)(L'0' + Version.MajorVersion),
                                        (WCHAR)(L'0' + Version.MinorVersion));
        if (!NT_SUCCESS(Status))
            goto Fallback;
        Buffer->FileSystemNameLength = FsNameSize;
    }
    else
    {
        // Report "NTFS x.x, Client x.x"
        const UNICODE_STRING Format = RTL_CONSTANT_STRING(L"NTFS %c.%c, Client %c.%c");
        const ULONG FsNameSize = Format.Length - (4 * sizeof(WCHAR));
        NtfsLfsClientVersion LfsClientVersion;
        PNtfsLogFileService Lfs = NtfsVolumeGetLFS(DiskVolume);
        NtfsLogFileServiceGetClientVersion(Lfs, &LfsClientVersion);
        BytesToWrite = FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + FsNameSize;
        if (*Length < BytesToWrite)
            goto Fallback;
        RtlInitEmptyUnicodeString(&NTFSVer,
                                  Buffer->FileSystemName,
                                  FsNameSize);
        Status = RtlUnicodeStringPrintf(&NTFSVer,
                                        Format.Buffer,
                                        (WCHAR)(L'0' + Version.MajorVersion),
                                        (WCHAR)(L'0' + Version.MinorVersion),
                                        (WCHAR)(L'0' + LfsClientVersion.MajorVersion),
                                        (WCHAR)(L'0' + LfsClientVersion.MinorVersion));
        if (!NT_SUCCESS(Status))
            goto Fallback;
        Buffer->FileSystemNameLength = FsNameSize;
    }
    goto Complete;

Fallback: ;
    // Report "NTFS"
    const ULONG FsNameSize = 8;
    BytesToWrite = FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + FsNameSize;
    if (*Length < BytesToWrite)
        return STATUS_BUFFER_TOO_SMALL;
    Buffer->FileSystemNameLength = FsNameSize;
    RtlCopyMemory(Buffer->FileSystemName, L"NTFS", FsNameSize);
    *Length -= BytesToWrite;

Complete:
    /* For more information on FileSystemAttributes:
     * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-smb/3065351b-0b78-4976-9a5a-11657d8857c7
     *
     * TODO: Add attributes as needed.
     */
    Buffer->FileSystemAttributes = FILE_CASE_PRESERVED_NAMES
                                   | FILE_UNICODE_ON_DISK
                                   | FILE_NAMED_STREAMS;

    if (NtfsVolumeIsReadOnly(DiskVolume))
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
    PNtfsVolume DiskVolume;

    DiskVolume = ((PVolumeContextBlock)(DeviceObject->DeviceExtension))->DiskVolume;

    if (!DiskVolume || !NewLabel)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = NtfsVolumeSetVolumeLabel(DiskVolume,
                                      NewLabel->VolumeLabel,
                                      NewLabel->VolumeLabelLength);

    if (!NT_SUCCESS(Status))
        return Status;

    // Re-read volume label.
    Status = NtfsVolumeGetVolumeLabel(DiskVolume,
                                      DeviceObject->Vpb->VolumeLabel,
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
            Status = NtfsGetAttributeInfo(VolCB->DiskVolume,
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
NTSTATUS
NTAPI
NtfsFsdSetVolumeInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                            _Inout_ PIRP Irp)
{
    /* Overview:
     * Handles requests to change file system information.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-set-volume-information
     */

    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack = IoGetCurrentIrpStackLocation(Irp);
    FS_INFORMATION_CLASS FSInfoRequest = IoStack->Parameters.QueryVolume.FsInformationClass;
    PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG BufferLength = IoStack->Parameters.QueryFile.Length;
    // PVolumeContextBlock VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;

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
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject)
{
    NTSTATUS Status;
    PDEVICE_OBJECT FSDeviceObject;
    PVolumeContextBlock VolCB;
    PNtfsVolume DiskVolume;
    NtfsDevice Device;
    NtfsConfiguration Configuration;

    // Set up NTFS library to use disk routines.
    Status = NtfsDiskInitializeKm(TargetDeviceObject,
                                  &Device);
    if (!NT_SUCCESS(Status))
        return Status;

    // Populate NTFS configuration settings
    Configuration.IsReadOnly = FALSE; // TODO: add reg key to set NTFS FSD to readonly
    Configuration.MftZoneReservation = gMftZoneReservation;
    Configuration.ShowHiddenMetadataFiles = gShowHiddenMetadataFiles;
    Configuration.UseLegacyMftZoneScaling = gUseLegacyMftZoneScaling;

    /* Check if we're really NTFS. It's OK if we're not. We're a boot driver,
     * NT will try every possible filesystem.
     *
     * If the client version is not compatible with our Ntfslib, it will return
     * STATUS_LOG_BLOCK_VERSION and set configuration to readonly. Do not exit
     * on this failure. Continue to open the volume as readonly.
     */
    Status = NtfsProbePartition(&Device,
                                &Configuration,
                                &DiskVolume,
                                NULL);
    if (!NT_SUCCESS(Status) && Status != STATUS_LOG_BLOCK_VERSION)
        return Status;

    // Currently used for debugging output.
    // DiskVolume->RunSanityChecks();

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

    // Do not set DO_BUFFERED_IO and DO_DIRECT_IO as a FS; leave to I/O manager/CC

    // Set up FastIo dispatch table for this volume
    FSDeviceObject->DriverObject->FastIoDispatch = &FastIoDispatch;

    // Initialize Volume Context Block VolCB.
    VolCB = (PVolumeContextBlock)FSDeviceObject->DeviceExtension;
    RtlZeroMemory(VolCB, sizeof(VolumeContextBlock));
    FSDeviceObject->Vpb = TargetDeviceObject->Vpb;

    VolCB->DiskVolume = DiskVolume;
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

    // Get serial number.
    FSDeviceObject->Vpb->SerialNumber = NtfsVolumeGetSerialNumber(DiskVolume);

    // Get volume label.
    Status = NtfsVolumeGetVolumeLabel(DiskVolume,
                                      FSDeviceObject->Vpb->VolumeLabel,
                                      &FSDeviceObject->Vpb->VolumeLabelLength);

    // Mount volume.
    FsRtlNotifyVolumeEvent(VolCB->StreamFileObject, FSRTL_VOLUME_MOUNT);

    return STATUS_SUCCESS;
}
