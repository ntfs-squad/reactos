/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"

NTSTATUS
Volume::LoadNTFSDevice(_In_ PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    NTSTATUS Status;
    ULONG ClusterSize, Size;
    USHORT i;
    BootSector* PartBootSector;
    PFileRecord VolumeFile = NULL;
    PVolumeInformationEx VolumeInfo;
    PAttribute VolumeInfoAttribute;

    PartDeviceObj = DeviceToMount;

    Size = sizeof(DISK_GEOMETRY);
    Status = DeviceIoControl(DeviceToMount,
                             IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             NULL,
                             0,
                             &DiskGeometry,
                             &Size,
                             TRUE);
    if (Status != STATUS_SUCCESS)
    {
        DPRINT1("NtfsDeviceIoControl() failed (Status %lx)\n", Status);
        __debugbreak(); //ASSERT?
    }

    // Check if we are actually NTFS.

    // Check bytes per sector.
    if (DiskGeometry.BytesPerSector != 512
        && DiskGeometry.BytesPerSector != 4096)
    {
        /* NOTE:
         * Per Microsoft's documentation, bytes per sector can either be
         * 4096 bytes or 512 bytes.
         *
         * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iobuildsynchronousfsdrequest
         */
        DPRINT1("Volume has invalid sector size! (%ld bytes)\n", DiskGeometry.BytesPerSector);
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    // Get boot sector information.
    PartBootSector = new(PagedPool) BootSector();
    if (!PartBootSector)
    {
        DPRINT1("Failed to allocate memory for boot sector!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = ReadDisk(DeviceToMount,
                      0,
                      DiskGeometry.BytesPerSector,
                      (PUCHAR)PartBootSector);

    if (!NT_SUCCESS(Status))
        goto Cleanup;

    // Check if OEM_ID is "NTFS    ".
    if (RtlCompareMemory(PartBootSector->OEM_ID, "NTFS    ", 8) != 8)
    {
        DPRINT1("Failed with NTFS-identifier: [%.8s]\n", PartBootSector->OEM_ID);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto Cleanup;
    }

    // Check if Reserved0 is NULL.
    for (i = 0; i < 7; i++)
    {
        if (PartBootSector->Reserved0[i] != 0)
        {
            DPRINT1("Failed in field Reserved0: [%.7s]\n", PartBootSector->Reserved0);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto Cleanup;
        }
    }

    // Check if Reserved3 is NULL.
    // TODO: Why doesn't this check work?
    /*for (i = 0; i < 7; i++)
    {
        if (PartBootSector->Reserved3[i] != 0)
        {
            DPRINT1("Failed in field Reserved3: [%.7s]\n", PartBootSector->Reserved3);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto Cleanup;
        }
    }*/

    // Check cluster size.
    ClusterSize = PartBootSector->BytesPerSector * PartBootSector->SectorsPerCluster;
    if (ClusterSize != 512 && ClusterSize != 1024 &&
        ClusterSize != 2048 && ClusterSize != 4096 &&
        ClusterSize != 8192 && ClusterSize != 16384 &&
        ClusterSize != 32768 && ClusterSize != 65536)
    {
        DPRINT1("Cluster size failed: %hu, %hu, %hu\n",
                PartBootSector->BytesPerSector,
                PartBootSector->SectorsPerCluster,
                ClusterSize);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto Cleanup;
    }

    // We are NTFS. Store only the boot sector information we need in memory.
#ifdef NTFS_DEBUG
    PrintNTFSBootSector(PartBootSector);
#endif

    RtlCopyMemory(&BytesPerSector,
                  &PartBootSector->BytesPerSector,
                  sizeof(UINT16));
    RtlCopyMemory(&SectorsPerCluster,
                  &PartBootSector->SectorsPerCluster,
                  sizeof(UINT8));
    ClustersInVolume = (PartBootSector->SectorsInVolume) / (PartBootSector->SectorsPerCluster);
    RtlCopyMemory(&ClustersPerIndexRecord,
                  &PartBootSector->ClustersPerIndexRecord,
                  sizeof(INT8));
    RtlCopyMemory(&SerialNumber,
                  &PartBootSector->SerialNumber,
                  sizeof(UINT64));

    // Initialize Master File Table
    MFT = new(PagedPool, TAG_MFT) MasterFileTable(this,
                                                  PartBootSector->MFTLCN,
                                                  PartBootSector->MFTMirrLCN,
                                                  PartBootSector->ClustersPerFileRecord);

    // Allocate Log File Service Object
    LFS = new(PagedPool, TAG_LOG_FILE_SERVICE) LogFileService(this);

    // Get the NTFS Major and Minor versions from $Volume.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeInformation,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeInfoAttribute);
    if (!NT_SUCCESS(Status))
        goto Cleanup;

    VolumeInfo = (PVolumeInformationEx)GetResidentDataPointer(VolumeInfoAttribute);

    // Set NTFS major and minor versions
    NtfsMajorVersion = VolumeInfo->MajorVersion;
    NtfsMinorVersion = VolumeInfo->MinorVersion;
    DPRINT1("NTFS Version %ld.%ld\n", VolumeInfo->MajorVersion, VolumeInfo->MinorVersion);

    // Initialize LFS
    Status = LFS->InitializeLFS();

    if (Status == STATUS_LOG_BLOCK_VERSION)
    {
        /* The version of LFS is incompatible with our LFS.
         * Open volume as readonly.
         */
        DPRINT1("Opening disk as readonly!\n");
        IsReadOnly = TRUE;
        Status = STATUS_SUCCESS;
    }

Cleanup:
    delete PartBootSector;
    if (VolumeFile)
        delete VolumeFile;
    return Status;
}

