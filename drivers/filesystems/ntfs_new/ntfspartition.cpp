#include "ntfs.h"
#include <ntdddisk.h>
#include <debug.h>

NtfsPartition::NtfsPartition(PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    NTSTATUS Status;
    ULONG Size;

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
    memcpy(&BYTES_PER_SECTOR,          &BootSector[0x0B], sizeof(UINT16));
    memcpy(&SECTORS_PER_CLUSTER,       &BootSector[0x0D], sizeof(UCHAR  ));
    memcpy(&MEDIA_DESCRIPTOR,          &BootSector[0x15], sizeof(UCHAR  ));
    memcpy(&SECTORS_PER_TRACK,         &BootSector[0x18], sizeof(UINT16 ));
    memcpy(&NUM_OF_HEADS,              &BootSector[0x1A], sizeof(UINT16 ));
    memcpy(&SECTORS_IN_VOLUME,         &BootSector[0x28], sizeof(UINT64));
    memcpy(&LCN_FOR_MFT,               &BootSector[0x30], sizeof(UINT64));
    memcpy(&LCN_FOR_MFT_MIRR,          &BootSector[0x38], sizeof(UINT64));
    memcpy(&CLUSTERS_PER_MFT_RECORD,   &BootSector[0x40], sizeof(UINT32));
    memcpy(&CLUSTERS_PER_INDEX_RECORD, &BootSector[0x44], sizeof(UINT32));
    memcpy(&VOLUME_SERIAL_NUMBER,      &BootSector[0x48], sizeof(UINT64));

    DPRINT1("OEM ID          %s\n", OEM_ID);
    DPRINT1("Bytes per sector %ld\n", BYTES_PER_SECTOR);
    DPRINT1("Sectors per cluster %ld\n", SECTORS_PER_CLUSTER);
   // PrintDetail("Media Descriptor", MediaDescriptorBuffer, false);
    DPRINT1("Sectors per track %ld\n", SECTORS_PER_TRACK);
    DPRINT1("Number of heads   %ld\n", NUM_OF_HEADS);
    DPRINT1("Sectors in volume %ld\n", SECTORS_IN_VOLUME);
    DPRINT1("LBA for $MFT      %ld\n", LCN_FOR_MFT);
    DPRINT1("LBA for $MFT_MIRR %ld\n", LCN_FOR_MFT_MIRR);
    DPRINT1("Clusters/MFT Rec %ld\n", CLUSTERS_PER_MFT_RECORD);
    DPRINT1("Clusters/IndexRec %ld\n", CLUSTERS_PER_INDEX_RECORD);
    DPRINT1("Volume serial number 0x%X\n", VOLUME_SERIAL_NUMBER);
  //  PrintDetail("Volume Size       ", VolumeSizeDescriptorBuffer, false);
    __debugbreak();
}

NtfsPartition::~NtfsPartition()
{

}

void
NtfsPartition::CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject)
{

}