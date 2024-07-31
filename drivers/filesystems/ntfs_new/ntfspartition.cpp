#include "io/ntfsprocs.h"
#include <ntdddisk.h>
#include <debug.h>
#include "ntfsdbgprint.h"
#include "mft.h"


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
    RtlCopyMemory(&ClustersPerFileRecord,
                  &PartBootSector->ClustersPerFileRecord,
                  sizeof(INT8));
    RtlCopyMemory(&ClustersPerIndexRecord,
                  &PartBootSector->ClustersPerIndexRecord,
                  sizeof(INT8));
    RtlCopyMemory(&SerialNumber,
                  &PartBootSector->SerialNumber,
                  sizeof(UINT64));

    // Load MFT
    VolMFT = new(NonPagedPool) MFT(this);

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
NtfsPartition::GetVolumeLabel(_In_ PWSTR VolumeLabel,
                              _In_ USHORT& Length)
{
    NTSTATUS Status;
    FileRecord* VolumeFileRecord;
    ResidentAttribute* VolumeNameAttr;

    VolumeFileRecord = new(NonPagedPool) FileRecord();
    VolumeNameAttr = new(NonPagedPool) ResidentAttribute();

    Status = VolMFT->GetFileRecord(_Volume, VolumeFileRecord);

    if (Status != STATUS_SUCCESS)
        goto Cleanup;

    Status = VolumeFileRecord->FindAttribute(VolumeName,
                                             NULL,
                                             VolumeNameAttr,
                                             (PUCHAR)VolumeLabel);

    if (Status != STATUS_SUCCESS)
        goto Cleanup;

    // Add null-terminator
    VolumeLabel[VolumeNameAttr->AttributeLength / sizeof(WCHAR)] = '\0';

    Length = VolumeNameAttr->AttributeLength;

Cleanup:
    delete VolumeFileRecord;
    delete VolumeNameAttr;
    return Status;
}

#include <debug.h>

/* SEPARATING OUT FOR SANITY */

void
NtfsPartition::RunSanityChecks()
{
    PAGED_CODE();

    DPRINT1("RunSanityChecks() called\n");
    DPRINT1("I don't have anything for now...\n");
}

NtfsPartition::~NtfsPartition()
{

}

void
NtfsPartition::CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject)
{

}