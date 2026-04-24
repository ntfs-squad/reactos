/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

EXTERN_C
NTSTATUS
NtfsProbePartition(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData)
{
    BootSector* PartitionBootSector;
    ULONG ClusterSize;
    USHORT i;

    // Check bytes per sector.
    if (BytesPerSector != 512
        && BytesPerSector != 4096)
    {
        /* NOTE:
         * Per Microsoft's documentation, bytes per sector can either be
         * 4096 bytes or 512 bytes.
         *
         * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iobuildsynchronousfsdrequest
         */
        DPRINT1("Volume has invalid sector size! (%ld bytes)\n", BytesPerSector);
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    PartitionBootSector = (BootSector*)BootSectorData;

    // Check if OEM_ID is "NTFS    ".
    if (RtlCompareMemory(PartitionBootSector->OEM_ID, "NTFS    ", 8) != 8)
    {
        DPRINT1("Failed with NTFS identifier: [%.8s]\n", PartitionBootSector->OEM_ID);
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    // Check if Reserved0 is NULL.
    for (i = 0; i < 7; i++)
    {
        if (PartitionBootSector->Reserved0[i] != 0)
        {
            DPRINT1("Failed in field Reserved0: [%.7s]\n", PartitionBootSector->Reserved0);
            return STATUS_UNRECOGNIZED_VOLUME;
        }
    }

    // Check if Reserved3 is NULL.
    // TODO: Why doesn't this check work?
    /*for (i = 0; i < 7; i++)
    {
        if (PartitionBootSector->Reserved3[i] != 0)
        {
            DPRINT1("Failed in field Reserved3: [%.7s]\n", PartitionBootSector->Reserved3);
            return STATUS_UNRECOGNIZED_VOLUME;
        }
    }*/

    // Check cluster size.
    ClusterSize = PartitionBootSector->BytesPerSector
                * PartitionBootSector->SectorsPerCluster;

    if (ClusterSize != 512 && ClusterSize != 1024 &&
        ClusterSize != 2048 && ClusterSize != 4096 &&
        ClusterSize != 8192 && ClusterSize != 16384 &&
        ClusterSize != 32768 && ClusterSize != 65536)
    {
        DPRINT1("Cluster size failed: %hu, %hu, %hu\n",
                PartitionBootSector->BytesPerSector,
                PartitionBootSector->SectorsPerCluster,
                ClusterSize);
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    // We are NTFS.
#ifdef NTFS_DEBUG
    PrintNTFSBootSector(PartitionBootSector);
#endif
    return STATUS_SUCCESS;
}

EXTERN_C
NTSTATUS
NtfsProbePartitionAndOpenVolume(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData,
    _Out_ void** VolumeOut)
{
    NTSTATUS Status;
    PVolume VolumeObject;

    // Make sure volume pointer is null if we fail for any reason.
    *VolumeOut = NULL;
    VolumeObject = NULL;

    Status = NtfsProbePartition(BytesPerSector, BootSectorData);
    if (!NT_SUCCESS(Status))
        return Status;

    // Initialize the volume object.
    VolumeObject = new(NonPagedPool) Volume();
    if (!VolumeObject)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = VolumeObject->Initialize(BootSectorData);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to initialize volume object! (Status %lx)\n", Status);
        delete VolumeObject;
        return Status;
    }

    *VolumeOut = VolumeObject;
    return Status;
}

Volume::~Volume()
{
    delete MFT;
    delete LFS;
}

NTSTATUS
Volume::Initialize(_In_ PUCHAR BootSectorData)
{
    BootSector* PartitionBootSector;
    PVolumeInformationEx VolumeInfo;
    PAttribute VolumeInfoAttribute;
    PFileRecord VolumeFile;
    NTSTATUS Status;
    
    PartitionBootSector = (BootSector*)BootSectorData;
    VolumeFile = NULL;

    // Pull in relevant information from the boot sector.
    BytesPerSector = PartitionBootSector->BytesPerSector;
    SectorsPerCluster = PartitionBootSector->SectorsPerCluster;
    ClustersInVolume = (PartitionBootSector->SectorsInVolume) / (PartitionBootSector->SectorsPerCluster);
    ClustersPerIndexRecord = PartitionBootSector->ClustersPerIndexRecord;
    SerialNumber = PartitionBootSector->SerialNumber;

    // Initialize Master File Table
    MFT = new(PagedPool, TAG_MFT) MasterFileTable(this,
                                                  PartitionBootSector->MFTLCN,
                                                  PartitionBootSector->MFTMirrLCN,
                                                  PartitionBootSector->ClustersPerFileRecord);
    if (!MFT)
        return STATUS_INSUFFICIENT_RESOURCES;
    
    // Get the NTFS Major and Minor versions from $Volume.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeInformation,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeInfoAttribute);
    
    if (!NT_SUCCESS(Status))
        return Status;
    
    VolumeInfo = (PVolumeInformationEx)GetResidentDataPointer(VolumeInfoAttribute);
    
    NtfsMajorVersion = VolumeInfo->MajorVersion;
    NtfsMinorVersion = VolumeInfo->MinorVersion;
    DPRINT1("NTFS Version %ld.%ld\n", VolumeInfo->MajorVersion, VolumeInfo->MinorVersion);
    
    // Initialize Log File Service
    LFS = new(PagedPool, TAG_LOG_FILE_SERVICE) LogFileService(this);

    if (!LFS)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = LFS->InitializeLFS();

    if (Status == STATUS_LOG_BLOCK_VERSION)
    {
        /* The version of LFS is incompatible with our LFS.
        * Open volume as readonly.
        */
        DPRINT1("LFS version is incompatible. Opening disk as readonly.\n");
        IsReadOnly = TRUE;
        Status = STATUS_SUCCESS;
    }

    return Status;
}
