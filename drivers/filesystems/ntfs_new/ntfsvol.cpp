/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "io/ntfsprocs.h"

// Macros for GetFreeClusters
const UINT8 Zeros[16] = { 4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0 };
#define GetZerosFromNibble(x) Zeros[(UINT8)x]
#define GetZerosFromByte(x) GetZerosFromNibble(x & 0xF) + GetZerosFromNibble(x >> 4)

// Min and max cluster sizes
#define MIN_CLUSTER_SIZE 512
#define MAX_CLUSTER_SIZE 65536

// Buffer used to read from disk
UCHAR DiskBuffer[MAX_CLUSTER_SIZE];

NTSTATUS
NTFSVolume::LoadNTFSDevice(_In_ PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    NTSTATUS Status;
    ULONG ClusterSize, Size;
    USHORT i;
    BootSector* PartBootSector;

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
    if (DiskGeometry.BytesPerSector > 512)
        return STATUS_UNRECOGNIZED_VOLUME;

    // Get boot sector information.
    PartBootSector = new(NonPagedPool) BootSector();
    ReadBlock(DeviceToMount,
              0,
              1,
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
    PrintNTFSBootSector(PartBootSector);

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

    // Initialize MFT
    MasterFileTable = new(PagedPool) MFT(this,
                                         PartBootSector->MFTLCN,
                                         PartBootSector->MFTMirrLCN,
                                         PartBootSector->ClustersPerFileRecord);

Cleanup:
    delete PartBootSector;
    return Status;
}

NTSTATUS
NTFSVolume::GetVolumeLabel(_Inout_ PWCHAR VolumeLabel,
                           _Inout_ PUSHORT Length)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PFileRecord VolumeFileRecord;
    PAttribute VolumeNameAttr;
    UINT32 AttrLength;

    // Allocate memory for $Volume file record and retrieve the file record.
    Status = MasterFileTable->GetFileRecord(_Volume, &VolumeFileRecord);

    // Clean up if failed.
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to find $Volume file\n");
        return STATUS_NOT_FOUND;
    }

    // Get pointer for the VolumeName attribute.
    VolumeNameAttr = VolumeFileRecord->GetAttribute(TypeVolumeName, NULL);

    if (!VolumeNameAttr)
    {
        // We didn't find the attribute. Abort.
        // TODO: Check the backup $Volume file record.
        DPRINT1("Failed to find $VOLUME_NAME attribute\n");
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    AttrLength = VolumeNameAttr->Resident.DataLength;

    // Copy volume name into VolumeLabel.
    RtlCopyMemory(VolumeLabel,
                  GetResidentDataPointer(VolumeNameAttr),
                  AttrLength);

    // Add null-terminator.
    VolumeLabel[AttrLength / sizeof(WCHAR)] = '\0';

    // Set length to attribute length.
    *Length = AttrLength;

cleanup:
    if (VolumeFileRecord)
        delete VolumeFileRecord;
    return Status;
}

// NTSTATUS
// NTFSVolume::WriteFileRecord(_In_ ULONGLONG FileRecordNumber,
//                                _In_ FileRecord* File)
// {
//     PAGED_CODE();
//     INT FileRecordOffset;

//     FileRecordOffset = (FileRecordNumber * FileRecordSize) / BytesPerSector;

//     // Warning: insane!
//     WriteBlock(PartDeviceObj,
//                (MFTLCN * SectorsPerCluster) + FileRecordOffset,
//                FileRecordSize / BytesPerSector,
//                BytesPerSector,
//                File->Data);

//     if (FileRecordNumber <= 4)
//     {
//         // The first 4 records are always duplicated in MFT Mirror
//         // See: https://flatcap.github.io/linux-ntfs/ntfs/files/mftmirr.html
//         // TODO: Larger disks duplicate more file records.

//         DPRINT1("This should also be written to $MftMirr, but isn't so I can compare.\n");
//         // Warning: also insane!
//         // WriteBlock(PartDeviceObj,
//         //            (MFTMirrLCN * SectorsPerCluster) + FileRecordOffset,
//         //            FileRecordSize / BytesPerSector,
//         //            BytesPerSector,
//         //            File->Data);
//     }

//     return STATUS_SUCCESS;
// }

NTSTATUS
NTFSVolume::SetVolumeLabel(_In_ PWCHAR VolumeLabel,
                              _In_ USHORT Length)
{
    NTSTATUS Status;
    FileRecord* VolumeFileRecord;
    PAttribute VolumeNameAttr;

    // Allocate memory for $Volume file record and retrieve the file record.
    Status = MasterFileTable->GetFileRecord(_Volume, &VolumeFileRecord);

    // Clean up if failed.
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to find $Volume file!\n");
        goto cleanup;
    }

    // Get pointer for $VolumeName attribute.
    VolumeNameAttr = VolumeFileRecord->GetAttribute(TypeVolumeName, NULL);

#if 0
    VolumeFileRecord->Header->ActualSize -= VolumeNameAttr->Length;

    VolumeNameAttr->AttributeLength = Length;
    VolumeNameAttr->Length = Length + sizeof(ResidentAttribute);

    VolumeFileRecord->Header->ActualSize += VolumeNameAttr->Length;

    DPRINT1("Let's look at the label we just copied: \"%S\"\n", GetResidentDataPointer(VolumeNameAttr));

    VolumeFileRecord->UpdateResidentAttribute(VolumeNameAttr);
#else
    // Copy new volume label into the $VolumeName attribute.
    // HACK! We don't move around the data structure yet.
    ASSERT(Length <= VolumeNameAttr->Resident.DataLength);
    RtlCopyMemory(GetResidentDataPointer(VolumeNameAttr),
                  VolumeLabel,
                  Length);
#endif

    // Overwrite the file record.
    // Status = WriteFileRecord(_Volume, VolumeFileRecord);

cleanup:
    delete VolumeFileRecord;
    return Status;
}

NTSTATUS
NTFSVolume::GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters)
{
    // Note: $Bitmap is *always* non-resident on Windows.
    NTSTATUS Status;
    FileRecord* BitmapFileRecord;
    PAttribute BitmapData, BitmapFileName;
    FileNameEx* BitmapFileNameEx;
    PDataRun DRHead, DRCurrent;
    UINT64 BytesToRead, ClusterReadSize;
    ULONG BytesPerCluster, ClusterPtr;

    // Get file record for $Bitmap
    Status = MasterFileTable->GetFileRecord(_Bitmap, &BitmapFileRecord);

    if (!BitmapFileRecord)
    {
        DPRINT1("Failed to get $Bitmap file!\n");
        goto cleanup;
    }

    // Calculate bytes per cluster
    BytesPerCluster = BytesPerSector * SectorsPerCluster;

    // Get pointers for $Bitmap to get data runs and file size.
    BitmapData = BitmapFileRecord->GetAttribute(TypeData, NULL);
    BitmapFileName = BitmapFileRecord->GetAttribute(TypeFileName, NULL);
    BitmapFileNameEx = (FileNameEx*)(GetResidentDataPointer(BitmapFileName));

    if (!BitmapData | !BitmapFileName | !BitmapFileNameEx)
    {
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    // Get the size of $Bitmap
    BytesToRead = BitmapFileNameEx->DataSize;

    // Loop through data runs to calculate free space
    DRHead = BitmapFileRecord->FindNonResidentData(BitmapData);
    DRCurrent = DRHead;
    FreeClusters->QuadPart = 0;

    while(DRCurrent)
    {
        ClusterPtr = 0;

        while (ClusterPtr < DRCurrent->Length)
        {
            // Preliminarily set the cluster read size to the number of bytes per cluster.
            ClusterReadSize = BytesPerCluster;

            // Get current LCN
            ReadBlock(PartDeviceObj,
                      (DRCurrent->LCN) + ClusterPtr,
                      1,
                      BytesPerCluster,
                      DiskBuffer);

            // Adjust cluster read size and the number of bytes to read according to actual file size.
            if (ClusterReadSize > BytesToRead)
                ClusterReadSize = BytesToRead;

            else
                BytesToRead -= ClusterReadSize;

            // Count number of unset bits and add to *FreeClusters
            for (ULONGLONG i = 0; i < ClusterReadSize; i++)
                FreeClusters->QuadPart += GetZerosFromByte(DiskBuffer[i]);

            // Increment cluster pointer
            ClusterPtr++;
        }
        // Set up next data run.
        DRCurrent = DRCurrent->NextRun;
    }

    Status = STATUS_SUCCESS;

// We're done! Time to cleanup.
cleanup:
    FreeDataRun(DRHead);
    delete BitmapFileRecord;
    return Status;
}

/* SEPARATING OUT FOR SANITY */
void
NTFSVolume::SanityCheckBlockIO()
{

    DPRINT1("Running a very close sanity check by reading one block, writing one block and re reading\n\n\n\n");
    UCHAR ReadBuffer[512] = {0};
    UCHAR PostWriteBuffer[512] = {0};
    UCHAR ZeroOutBuffer[512] = {0};
        //save disk
       ReadBlock(PartDeviceObj,
                     1,
                     1,
                     BytesPerSector,
                     (PUCHAR)ReadBuffer);

        //erase disk
        WriteBlock(PartDeviceObj,
            1,
            1,
            BytesPerSector,
            ZeroOutBuffer);
        KeStallExecutionProcessor(100);
        //recover disk
        WriteBlock(PartDeviceObj,
            1,
            1,
            BytesPerSector,
            ReadBuffer);
        //verify
       ReadBlock(PartDeviceObj,
                     1,
                     1,
                     BytesPerSector,
                     (PUCHAR)PostWriteBuffer);
    for (int i = 0; i < 512; i++)
    {
        DPRINT1("ReadBuffer at Location %d, is value: %X\n", i, ReadBuffer[i]);
        DPRINT1("PostWriteBuffer at Location %d, is value: %X\n", i, PostWriteBuffer[i]);

        if (ReadBuffer[i] == PostWriteBuffer[i])
        {
            DPRINT1("Sanity Check passed for iteration %d\n", i);
        }
        else
        {
            __debugbreak();
        }
    }
}
void
NTFSVolume::RunSanityChecks()
{
    PAGED_CODE();

    DPRINT1("RunSanityChecks() called\n");
    // SanityCheckBlockIO();

// Wipe drive
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

//SanityCheck IO calls

}

NTFSVolume::~NTFSVolume()
{


}

void
NTFSVolume::CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject)
{

}