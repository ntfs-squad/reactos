/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

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

    // Initialize Log File Service
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

Cleanup:
    delete PartBootSector;
    if (VolumeFile)
        delete VolumeFile;
    return Status;
}

NTSTATUS
NTFSVolume::GetVolumeLabel(_Inout_ PWSTR VolumeLabel,
                           _Inout_ PUSHORT Length)
{
    NTSTATUS Status;
    PFileRecord VolumeFile;
    PAttribute VolumeNameAttr;
    UINT32 AttrLength;

    // Allocate memory for $Volume file record and retrieve the file record.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeName,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeNameAttr);
    if (!NT_SUCCESS(Status))
        return Status;

    AttrLength = VolumeNameAttr->Resident.DataLength;

    // Copy volume name into VolumeLabel.
    RtlCopyMemory(VolumeLabel,
                  GetResidentDataPointer(VolumeNameAttr),
                  AttrLength);

    // Add null-terminator.
    VolumeLabel[AttrLength / sizeof(WCHAR)] = '\0';

    // Set length to attribute length.
    *Length = AttrLength;

    if (VolumeFile)
        delete VolumeFile;
    return Status;
}

NTSTATUS
NTFSVolume::SetVolumeLabel(_In_ PWSTR VolumeLabel,
                           _In_ USHORT Length)
{
    NTSTATUS Status;
    FileRecord* VolumeFile;
    PAttribute VolumeNameAttr;

    /* Allocate memory for $Volume file record, retrieve the file record, and
     * get a pointer to the $VOLUME_NAME attribute.
     */
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeName,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeNameAttr);

    if (!NT_SUCCESS(Status))
        return Status;

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

    delete VolumeFile;
    return Status;
}

NTSTATUS
NTFSVolume::GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters)
{
    // Note: $Bitmap is *always* non-resident on Windows.
    NTSTATUS Status;
    PFileRecord BitmapFile;
    PAttribute BitmapData;
    ULONG BytesToRead;
    PUCHAR BitmapBuffer;

    // Get file record for $Bitmap and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _Bitmap,
                                                       &BitmapFile,
                                                       &BitmapData);

    if (!NT_SUCCESS(Status))
        return Status;

    // Get the size of $Bitmap
    BytesToRead = GetAttributeDataSize(BitmapData);

    // Initialize bitmap buffer
    BitmapBuffer = new(NonPagedPool) UCHAR[BytesToRead];

    // Copy attribute data into this buffer.
    BitmapFile->CopyData(BitmapData,
                         BitmapBuffer,
                         &BytesToRead);

    BytesToRead = GetAttributeDataSize(BitmapData) - BytesToRead;

    FreeClusters->QuadPart = 0;

    for (int i = 0; i < BytesToRead; i++)
    {
        FreeClusters->QuadPart += GetZerosFromByte(BitmapBuffer[i]);
    }

    Status = STATUS_SUCCESS;

// We're done! Time to cleanup.
    delete BitmapBuffer;
    delete BitmapFile;
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

    // Save disk
    ReadVolume(BytesPerSector,
               BytesPerSector,
               ReadBuffer);

    // Erase disk
    WriteVolume(BytesPerSector,
                BytesPerSector,
                ZeroOutBuffer);

    KeStallExecutionProcessor(100);

    // Recover disk
    WriteVolume(BytesPerSector,
                BytesPerSector,
                ReadBuffer);

    // Verify disk
    ReadVolume(BytesPerSector,
               BytesPerSector,
               PostWriteBuffer);

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

NTSTATUS
NTFSVolume::GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                                     _Out_ AttributeType* Type)
{
    NTSTATUS Status;
    PFileRecord AttrDefFile;
    PAttribute DataAttr;
    PAttrDefEntry TableEntry;
    ULONG AttrDefEntryIndex, AttrDefDataSize, MaxIndex, NameCompareLength;
    PUCHAR Buffer;

    // NOTE: Lookup is case-insensitive.

    // Get file record for $Bitmap and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _AttrDef,
                                                       &AttrDefFile,
                                                       &DataAttr);

    // If this fails, there's something majorly wrong with this drive.
    if (!NT_SUCCESS(Status))
        return Status;

#ifdef NTFS_DEBUG
    PrintAttrDefTable(AttrDefFile);
#endif

    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDefFile->CopyData(DataAttr,
                          Buffer,
                          &AttrDefDataSize,
                          0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    AttrDefEntryIndex = 0;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;
    NameCompareLength = wcslen(AttributeTypeName);

    // Uppercase the AttributeTypeName for case-insensitive matching
    for (int i = 0; i < NameCompareLength; i++)
        AttributeTypeName[i] = RtlUpcaseUnicodeChar(AttributeTypeName[i]);

    NameCompareLength *= sizeof(WCHAR);

    for (int i = 0; i < MaxIndex; i++)
    {
        if ((wcslen(TableEntry->Label) * sizeof(WCHAR)) == NameCompareLength &&
            RtlCompareMemory(TableEntry->Label,
                             AttributeTypeName,
                             NameCompareLength) == NameCompareLength)
        {
            // We found the attribute name!
            *Type = AttributeType(TableEntry->AttributeType);
            return STATUS_SUCCESS;
        }

        // Move onto the next element
        TableEntry++;
    }

    return STATUS_NOT_FOUND;
}
