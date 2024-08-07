#include "io/ntfsprocs.h"

UCHAR FileRecordBuffer[0x100000]; // TODO: Figure proper size.

// Macros for GetFreeClusters.
const UINT8 Zeros[16] = { 4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0 };
#define GetZerosFromNibble(x) Zeros[(UINT8)x]
#define GetZerosFromByte(x) GetZerosFromNibble(x & 0xF) + GetZerosFromNibble(x >> 4)

// Min and max cluster sizes
#define MIN_CLUSTER_SIZE 512
#define MAX_CLUSTER_SIZE 65536

NTSTATUS
NtfsPartition::LoadNtfsDevice(_In_ PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    NTSTATUS Status;
    ULONG ClusterSize, Size;
    USHORT i;
    BootSector* PartBootSector;

    DPRINT1("Loading NTFS Device...\n");
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

    DPRINT1("Got Drive Geometry!...\n");

    // Check if we are actually NTFS.
    // Check bytes per sector.
    if (DiskGeometry.BytesPerSector > 512)
        return STATUS_UNRECOGNIZED_VOLUME;

    DPRINT1("Bytes per sector passed!...\n");

    // Get boot sector information.
    PartBootSector = new(NonPagedPool) BootSector();
    ReadBlock(DeviceToMount,
              0,
              1,
              DiskGeometry.BytesPerSector,
              (PUCHAR)PartBootSector,
              TRUE);

    if (!NT_SUCCESS(Status))
        goto Cleanup;

    // Check if OEM_ID is "NTFS    ".
    if (RtlCompareMemory(PartBootSector->OEM_ID, "NTFS    ", 8) != 8)
    {
        DPRINT1("Failed with NTFS-identifier: [%.8s]\n", PartBootSector->OEM_ID);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto Cleanup;
    }

    DPRINT1("OEM ID is NTFS!...\n");

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

    DPRINT1("Cluster size passed!...\n");

    // We are NTFS.
    PrintNTFSBootSector(PartBootSector);

    // Store only the boot sector information we need in memory.
    RtlCopyMemory(&BytesPerSector,
                  &PartBootSector->BytesPerSector,
                  sizeof(UINT16));
    RtlCopyMemory(&SectorsPerCluster,
                  &PartBootSector->SectorsPerCluster,
                  sizeof(UINT8));
    ClustersInVolume = (PartBootSector->SectorsInVolume) / (PartBootSector->SectorsPerCluster);
    RtlCopyMemory(&MFTLCN,
                  &PartBootSector->MFTLCN,
                  sizeof(UINT64));
    RtlCopyMemory(&MFTMirrLCN,
                  &PartBootSector->MFTMirrLCN,
                  sizeof(UINT64));
    RtlCopyMemory(&ClustersPerIndexRecord,
                  &PartBootSector->ClustersPerIndexRecord,
                  sizeof(INT8));
    RtlCopyMemory(&SerialNumber,
                  &PartBootSector->SerialNumber,
                  sizeof(UINT64));

    /* Get File Record Size (Bytes).
     * If clusters per file record is less than 0, the file record size is 2^(-ClustersPerFileRecord).
     * Otherwise, the file record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector.
     */
    FileRecordSize = PartBootSector->ClustersPerFileRecord < 0 ?
                     1 << (-(PartBootSector->ClustersPerFileRecord)) :
                     PartBootSector->ClustersPerFileRecord * SectorsPerCluster * BytesPerSector;

Cleanup:
    delete PartBootSector;
    return Status;
}

NTSTATUS
NtfsPartition::DumpBlocks(_Inout_ PUCHAR Buffer,
                          _In_    ULONG Lba,
                          _In_    ULONG LbaCount)
{
    return ReadBlock(PartDeviceObj,
                     Lba,
                     LbaCount,
                     BytesPerSector,
                     (PUCHAR)Buffer,
                     TRUE);
}

NTSTATUS
NtfsPartition::GetFileRecord(_In_  ULONGLONG FileRecordNumber,
                             _Out_ FileRecord* File)
{
    PAGED_CODE();

    INT FileRecordOffset;

    FileRecordOffset = (FileRecordNumber * FileRecordSize) / BytesPerSector;

    DumpBlocks(FileRecordBuffer,
               (MFTLCN * SectorsPerCluster) + FileRecordOffset,
               FileRecordSize / BytesPerSector);

    File->LoadData(FileRecordBuffer,
                   FileRecordSize);

    return STATUS_SUCCESS;
}

NTSTATUS
NtfsPartition::GetVolumeLabel(_Inout_ PWCHAR VolumeLabel,
                              _Inout_ PUSHORT Length)
{
    NTSTATUS Status;
    FileRecord* VolumeFileRecord;
    ResidentAttribute* VolumeNameAttr;
    UINT32 AttrLength;

    // Allocate memory for $Volume file record.
    VolumeFileRecord = new(NonPagedPool) FileRecord();

    // Retrieve file record.
    Status = GetFileRecord(_Volume, VolumeFileRecord);

    // Clean up if failed.
    if (Status != STATUS_SUCCESS)
        goto cleanup;

    // Get pointer for the VolumeName attribute.
    VolumeNameAttr = (ResidentAttribute*)VolumeFileRecord->FindAttributePointer(VolumeName, NULL);

    if (!VolumeNameAttr)
    {
        // We didn't find the attribute. Abort.
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    AttrLength = VolumeNameAttr->AttributeLength;

    // Copy volume name into VolumeLabel.
    RtlCopyMemory(VolumeLabel,
                  GetResidentDataPointer(VolumeNameAttr),
                  AttrLength);

    // Add null-terminator.
    VolumeLabel[AttrLength / sizeof(WCHAR)] = '\0';

    // Set length to attribute length.
    *Length = AttrLength;

cleanup:
    delete VolumeFileRecord;
    return Status;
}


// TODO: what is wrong here?
NTSTATUS
NtfsPartition::GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters)
{
    // Note: $Bitmap is *always* non-resident on Windows.
    NTSTATUS Status;
    FileRecord* BitmapFileRecord;
    NonResidentAttribute* BitmapData;
    //ResidentAttribute* BitmapFileName;
    //FileNameEx* BitmapFNEx;
    //PDataRun DRHead, DRCurrent;
    //ULONGLONG LCN, LCN_MAX;
    //ULONG BytesPerCluster, BytePos, EndBytePosOnLastCluster;

    DPRINT1("Finding free clusters...\n");

    BitmapFileRecord = new(NonPagedPool) FileRecord();
    FreeClusters->QuadPart = 0;

    Status = GetFileRecord(_Bitmap, BitmapFileRecord);

    if (Status != STATUS_SUCCESS)
        goto cleanup;

    DPRINT1("Found it. Looking for Data attribute...\n");

    BitmapData = (NonResidentAttribute*)(BitmapFileRecord->FindAttributePointer(Data, NULL));

    if (!BitmapData)
        goto cleanup;

    DPRINT1("Found it. Printing attribute now...\n");

    PrintNonResidentAttributeHeader(BitmapData);

    DPRINT1("Finding FileName Attribute for $Bitmap...\n");

    // Dummy data for now...
    FreeClusters->QuadPart = 50000;

#if 0

    // Calculate Bytes per Cluster
    BytesPerCluster = BytesPerSector * SectorsPerCluster;

    DPRINT1("Bytes per Cluster: %ld\n", BytesPerCluster);

    //BitmapFileName = (ResidentAttribute*)(BitmapFileRecord->FindAttributePointer(FileName, NULL));
    //BitmapFNEx = ((FileNameEx*)GetResidentDataPointer(BitmapFileName));

    //EndBytePosOnLastCluster = BitmapFNEx->RealSize % BytesPerCluster;

    /* TODO:
     * - Read $Bitmap, each bit not set represents one free cluster.
     * - Free Clusters = Number of cluster bits not set.
     * NOTE: Windows' NTFS includes the MFT size in its free space estimate,
     *       so we should be safe to use the above algorithm.
     */

    // __debugbreak();

    // Get Non-Resident Data.
    DRHead = BitmapFileRecord->FindNonResidentData(BitmapData);
    DRCurrent = DRHead;

    while(DRCurrent)
    {
        // Get LCN, Length.
        LCN = DRCurrent->LCN;
        LCN_MAX = DRCurrent->LCN + DRCurrent->Length;

        while(LCN < LCN_MAX)
        {
            BytePos = 0;

            // Load Buffer
            ReadBlock(PartDeviceObj,
                      LCN,
                      1,
                      BytesPerCluster,
                      FileRecordBuffer,
                      TRUE);

            /* Count number of zeros in the cluster buffer. Add to *FreeClusters.
             * TODO: Consider applying loop unrolling to improve performance.
             *       $Bitmap uses 8-byte sections, so it's safe to get zeros from
             *       the next 8-bytes on every iteration.
             */

            while (BytePos < BytesPerCluster)
            {
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[1 + BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[2 + BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[3 + BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[4 + BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[5 + BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[6 + BytePos]);
                FreeClusters->QuadPart += GetZerosFromByte(FileRecordBuffer[7 + BytePos]);

                BytePos += 8;

                /* If we're in the last cluster, check if we reached EOF and stop counting. */
                if (!(DRCurrent->NextRun) &&
                    LCN == LCN_MAX - 1 &&
                    BytePos == EndBytePosOnLastCluster)
                {
                    // Break out of loop.
                    BytePos = BytesPerCluster;
                }
            }

            // Start searching the next cluster.
            LCN++;
        }

        // Set up next data run.
        DRCurrent = DRCurrent->NextRun;
    }
#endif

// We're done! Time to cleanup.
cleanup:
    //FreeDataRun(DRHead);
    delete BitmapFileRecord;
    return STATUS_SUCCESS;
}

/* SEPARATING OUT FOR SANITY */

void
NtfsPartition::RunSanityChecks()
{
    PAGED_CODE();

    DPRINT1("RunSanityChecks() called\n");
    DPRINT1("I don't have anything for now...\n");

#if 0

    WARNING THIS CODE INTENTIONALLY CORRUPTS THE HARDDRIVE
    UCHAR Buffer[512] = {0};

    for(int i = 0; i < 64; i ++)
    {
        DPRINT1("Erasing block\n");
   NTSTATUS Status =  WriteBlock(PartDeviceObj,
          i,
          1,
          512,
          Buffer);
          DPRINT1("Write block Status %X\n", Status);
    }
#endif
}

NtfsPartition::~NtfsPartition()
{

}

void
NtfsPartition::CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject)
{

}