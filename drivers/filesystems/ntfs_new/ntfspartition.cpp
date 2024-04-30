#include "ntfs.h"
#include <ntdddisk.h>
#include <debug.h>
#include "contextblocks.h"

NtfsPartition::NtfsPartition(PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    NTSTATUS Status;
    ULONG Size;
    UCHAR BootSector[512];

    PartDeviceObj = DeviceToMount;
    BlockIo = new(PagedPool) NtBlockIo();

    Size = sizeof(DISK_GEOMETRY);
    Status = BlockIo->DeviceIoControl(DeviceToMount,
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

    BytesPerSector = DiskGeometry.BytesPerSector;

    /* Get Volume Information */
    /* Get boot sector information */
    DumpBlocks(BootSector, 0,1);

    //memcpy(&BytesPerSector,         &BootSector[0x0B], sizeof(UINT16));
    memcpy(&SectorsPerCluster,      &BootSector[0x0D], sizeof(UINT8 ));
    memcpy(&SectorsInVolume,        &BootSector[0x28], sizeof(UINT64));
    memcpy(&MFTLCN,                 &BootSector[0x30], sizeof(UINT64));
    memcpy(&MFTMirrLCN,             &BootSector[0x38], sizeof(UINT64));
    memcpy(&ClustersPerFileRecord,  &BootSector[0x40], sizeof(UINT32));
    memcpy(&ClustersPerIndexRecord, &BootSector[0x44], sizeof(UINT32));
    memcpy(&SerialNumber,           &BootSector[0x48], sizeof(UINT64));

    /* Get $Volume information */

    /* Get Root File Object */
}

NTSTATUS
NtfsPartition::DumpBlocks(_Inout_ PUCHAR Buffer,
                          _In_    ULONG Lba,
                          _In_    ULONG LbaCount)
{
    return BlockIo->ReadBlock(PartDeviceObj,
                              Lba,
                              LbaCount,
                              BytesPerSector,
                              (PUCHAR)Buffer,
                              TRUE);
}















































#include <debug.h>

/* SEPERATING OUT FOR SNAITY */

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
    DPRINT1("RunSanityChecks() called\n");
    UCHAR BootSector[512];

    OEM_ID = new(NonPagedPool) char[9];
    //ReadSector(BootSector, 0);
    DumpBlocks(BootSector, 0,1);

    strcpy2(OEM_ID, BootSector, 0x03, 8);
    memcpy(&MEDIA_DESCRIPTOR,          &BootSector[0x15], sizeof(UCHAR ));
    memcpy(&SECTORS_PER_TRACK,         &BootSector[0x18], sizeof(UINT16));
    memcpy(&NUM_OF_HEADS,              &BootSector[0x1A], sizeof(UINT16));

    DPRINT1("OEM ID            %s\n", OEM_ID);
    DPRINT1("Bytes per sector  %ld\n", BytesPerSector);
    DPRINT1("Sectors/cluster   %ld\n", SectorsPerCluster);
    DPRINT1("Sectors per track %ld\n", SECTORS_PER_TRACK);
    DPRINT1("Number of heads   %ld\n", NUM_OF_HEADS);
    DPRINT1("Sectors in volume %ld\n", SectorsInVolume);
    DPRINT1("LCN for $MFT      %ld\n", MFTLCN);
    DPRINT1("LCN for $MFT_MIRR %ld\n", MFTMirrLCN);
    DPRINT1("Clusters/MFT Rec  %ld\n", ClustersPerFileRecord);
    DPRINT1("Clusters/IndexRec %ld\n", ClustersPerIndexRecord);
    DPRINT1("Serial number   0x%X\n", SerialNumber);
    DPRINT1("Volume label      \"%s\"\n", VolumeParameterBlock->VolumeLabel);
}

NtfsPartition::~NtfsPartition()
{

}

void
NtfsPartition::CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject)
{

}