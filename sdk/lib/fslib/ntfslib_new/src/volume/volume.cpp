/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#ifdef NTFS_DEBUG
static void PrintNTFSBootSector(PBootSector PartBootSector)
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
}
#endif

static BOOLEAN
IsValidRecordSize(_In_ INT8 ClustersPerRecord,
                  _In_ ULONG ClusterSize)
{
    ULONG RecordSize;

    if (ClustersPerRecord == 0)
        return FALSE;

    if (ClustersPerRecord < 0)
    {
        ULONG Exponent = (ULONG)(-ClustersPerRecord);
        if (Exponent >= 32)
            return FALSE;
        RecordSize = 1UL << Exponent;
    }
    else
    {
        if ((ULONG)ClustersPerRecord > MAXULONG / ClusterSize)
            return FALSE;
        RecordSize = (ULONG)ClustersPerRecord * ClusterSize;
    }

    return RecordSize >= 512 && RecordSize <= 65536 &&
           (RecordSize % sizeof(ULONGLONG)) == 0;
}

EXTERN_C
NTSTATUS
NtfsProbePartition(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData)
{
    BootSector* PartitionBootSector;
    ULONG ClusterSize;
    USHORT i;

    if (!BootSectorData)
        return STATUS_INVALID_PARAMETER;

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

    /* Check if Reserved3 is NULL.
     * Note: Reserved3 is only 4 bytes; reading 7 like Reserved0 walks into
     * the following (nonzero) field, which is why this check used to fail.
     */
    for (i = 0; i < 4; i++)
    {
        if (PartitionBootSector->Reserved3[i] != 0)
        {
            DPRINT1("Failed in field Reserved3: [%.4s]\n", PartitionBootSector->Reserved3);
            return STATUS_UNRECOGNIZED_VOLUME;
        }
    }

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

    if (!IsValidRecordSize(PartitionBootSector->ClustersPerFileRecord,
                           ClusterSize) ||
        !IsValidRecordSize(PartitionBootSector->ClustersPerIndexRecord,
                           ClusterSize))
    {
        DPRINT1("Volume has invalid file or index record sizes!\n");
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
    _Out_ PNtfsVolume* VolumeOut)
{
    NTSTATUS Status;
    PVolume VolumeObject;
    PUCHAR BootSectorData;

    if (!VolumeOut || BytesPerSector == 0)
        return STATUS_INVALID_PARAMETER;

    // Make sure volume pointer is null if we fail for any reason.
    *VolumeOut = NULL;

    // Read the boot sector through the environment's disk routines.
    BootSectorData = new(NonPagedPool) UCHAR[BytesPerSector];
    if (!BootSectorData)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = NtfsReadVolume(0, BytesPerSector, BootSectorData);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to read the boot sector! (Status %lx)\n", Status);
        goto Done;
    }

    Status = NtfsProbePartition(BytesPerSector, BootSectorData);
    if (!NT_SUCCESS(Status))
        goto Done;

    // Initialize the volume object.
    VolumeObject = new(NonPagedPool) Volume();
    if (!VolumeObject)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    Status = VolumeObject->Initialize(BootSectorData);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to initialize volume object! (Status %lx)\n", Status);
        delete VolumeObject;
        goto Done;
    }

    *VolumeOut = reinterpret_cast<PNtfsVolume>(VolumeObject);

Done:
    delete[] BootSectorData;
    return Status;
}

Volume::~Volume()
{
    delete MFT;
    delete LFS;
    delete[] AttrDefCache;
    delete[] UpcaseTable;
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
    ShowMetadataFiles = NtfsDefaultShowMetadataFiles;

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
    delete VolumeFile;
    VolumeFile = NULL;

    
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
