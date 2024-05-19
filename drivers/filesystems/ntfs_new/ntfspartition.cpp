#include "io/ntfsprocs.h"
#include <ntdddisk.h>
#include <debug.h>
#include "ntfsdbgprint.h"
#include "mft.h"


NTSTATUS NtfsPartition::LoadNtfsDevice(_In_ PDEVICE_OBJECT DeviceToMount)
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
    /* Check if we are actually NTFS. */

    /* Check bytes per sector. */
    if (DiskGeometry.BytesPerSector > 512)
        return STATUS_UNRECOGNIZED_VOLUME;

    DPRINT1("Bytes per sector passed!...\n");

    /* Get boot sector information. */
    PartBootSector = new(NonPagedPool) BootSector();
    ReadBlock(DeviceToMount,
              0,
              1,
              DiskGeometry.BytesPerSector,
              (PUCHAR)PartBootSector,
              TRUE);

    if (!NT_SUCCESS(Status))
        goto Cleanup;

    /* Check if OEM_ID is "NTFS    ". */
    if (RtlCompareMemory(PartBootSector->OEM_ID, "NTFS    ", 8) != 8)
    {
        DPRINT1("Failed with NTFS-identifier: [%.8s]\n", PartBootSector->OEM_ID);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto Cleanup;
    }

    DPRINT1("OEM ID is NTFS!...\n");

    /* Check if Reserved0 is NULL. */
    for (i = 0; i < 7; i++)
    {
        if (PartBootSector->Reserved0[i] != 0)
        {
            DPRINT1("Failed in field Reserved0: [%.7s]\n", PartBootSector->Reserved0);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto Cleanup;
        }
    }

    /* Check if Reserved3 is NULL. */
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

    /* Check cluster size. */
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

    /* We are NTFS. */
    PrintNTFSBootSector(PartBootSector);

    /* Store only the boot sector information we need in memory. */
    RtlCopyMemory(&BytesPerSector,
                  &PartBootSector->BytesPerSector,
                  sizeof(UINT16));
    RtlCopyMemory(&SectorsPerCluster,
                  &PartBootSector->SectorsPerCluster,
                  sizeof(UINT8));
    RtlCopyMemory(&SectorsInVolume,
                  &PartBootSector->SectorsInVolume,
                  sizeof(UINT64));
    RtlCopyMemory(&MFTLCN,
                  &PartBootSector->MFTLCN,
                  sizeof(UINT64));
    RtlCopyMemory(&MFTMirrLCN,
                  &PartBootSector->MFTMirrLCN,
                  sizeof(UINT64));
    RtlCopyMemory(&ClustersPerFileRecord,
                  &PartBootSector->ClustersPerFileRecord,
                  sizeof(UINT32));
    RtlCopyMemory(&ClustersPerIndexRecord,
                  &PartBootSector->ClustersPerIndexRecord,
                  sizeof(UINT32));
    RtlCopyMemory(&SerialNumber,
                  &PartBootSector->SerialNumber,
                  sizeof(UINT64));

Cleanup:
    ExFreePool(PartBootSector);

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


#include <debug.h>

/* SEPARATING OUT FOR SANITY */

void strcpy2(char* destination,
    UCHAR* source,
    unsigned int start,
    unsigned int length)
{
    for (int i = 0; i < length; i++)
        destination[i] = source[i + start];
    destination[length] = 0;
}

void
NtfsPartition::RunSanityChecks()
{
    PAGED_CODE();

    WCHAR Filename[256];
    WCHAR VolumeName[128];

    MFT *mft;
    FileRecord* VolumeFileRecord;
    ResidentAttribute* VolumeNameAttr;
    ResidentAttribute* FilenameAttrib;
    FileNameEx* FilenameExtAttr;

    DPRINT1("RunSanityChecks() called\n");

    mft = new(NonPagedPool) MFT(this);
    VolumeFileRecord = new(NonPagedPool) FileRecord();

    mft->GetFileRecord(_Volume, VolumeFileRecord);

    DPRINT1("We set up the file record...\n");

    FilenameAttrib = new(NonPagedPool) ResidentAttribute();
    FilenameExtAttr = new(NonPagedPool) FileNameEx();
    VolumeNameAttr = new(NonPagedPool) ResidentAttribute();

    DPRINT1("Finding Attribute...\n");

    VolumeFileRecord->FindFileNameAttribute(FilenameAttrib, FilenameExtAttr, Filename);
    VolumeFileRecord->FindVolumeNameAttribute(VolumeNameAttr, VolumeName);

    DPRINT1("Volume Name is: \"%S\"\n", VolumeName);
    DPRINT1("File name: \"%S\"\n", Filename);
}

NtfsPartition::~NtfsPartition()
{

}

void
NtfsPartition::CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject)
{

}