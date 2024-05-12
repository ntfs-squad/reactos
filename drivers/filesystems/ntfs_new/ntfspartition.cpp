#include "io/ntfsprocs.h"
#include <ntdddisk.h>
#include <debug.h>
#include "ntfsdbgprint.h"
#include "mft.h"


NtfsPartition::NtfsPartition(PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    NTSTATUS Status;
    ULONG Size;
    UCHAR BootSector[512];

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

    VCB->BytesPerSector = DiskGeometry.BytesPerSector;

    /* Get Volume Information */
    /* Get boot sector information */
    DumpBlocks(BootSector, 0,1);

    //memcpy(&BytesPerSector,         &BootSector[0x0B], sizeof(UINT16));
    RtlCopyMemory(&VCB->SectorsPerCluster,      &BootSector[0x0D], sizeof(UINT8 ));
    RtlCopyMemory(&VCB->SectorsInVolume,        &BootSector[0x28], sizeof(UINT64));
    RtlCopyMemory(&VCB->MFTLCN,                 &BootSector[0x30], sizeof(UINT64));
    RtlCopyMemory(&VCB->MFTMirrLCN,             &BootSector[0x38], sizeof(UINT64));
    RtlCopyMemory(&VCB->ClustersPerFileRecord,  &BootSector[0x40], sizeof(UINT32));
    RtlCopyMemory(&VCB->ClustersPerIndexRecord, &BootSector[0x44], sizeof(UINT32));
    RtlCopyMemory(&VCB->SerialNumber,           &BootSector[0x48], sizeof(UINT64));

    /* Get $Volume information */

    /* Get Root File Object */
}

NTSTATUS
NtfsPartition::DumpBlocks(_Inout_ PUCHAR Buffer,
                          _In_    ULONG Lba,
                          _In_    ULONG LbaCount)
{
    return ReadBlock(PartDeviceObj,
                     Lba,
                     LbaCount,
                     VCB->BytesPerSector,
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
    PAGED_CODE();

    UCHAR BootSector[512];
    WCHAR Filename[256];
    WCHAR VolumeName[128];

    MFT *mft;
    FileRecord* VolumeFileRecord;
    ResidentAttribute* VolumeNameAttr;
    ResidentAttribute* FilenameAttrib;
    FILE_NAME* FilenameExtAttr;

    DPRINT1("RunSanityChecks() called\n");
    OEM_ID = new(NonPagedPool) char[9];
    DumpBlocks(BootSector, 0,1);

    strcpy2(OEM_ID, BootSector, 0x03, 8);
    RtlCopyMemory(&MEDIA_DESCRIPTOR,  &BootSector[0x15], sizeof(UCHAR ));
    RtlCopyMemory(&SECTORS_PER_TRACK, &BootSector[0x18], sizeof(UINT16));
    RtlCopyMemory(&NUM_OF_HEADS,      &BootSector[0x1A], sizeof(UINT16));

    PrintVCB(VCB, OEM_ID, SECTORS_PER_TRACK, NUM_OF_HEADS);

    mft = new(NonPagedPool) MFT(VCB, PartDeviceObj);
    VolumeFileRecord = new(NonPagedPool) FileRecord();

    mft->GetFileRecord(_Volume, VolumeFileRecord);

    DPRINT1("We set up the file record...\n");

    FilenameAttrib = new(NonPagedPool) ResidentAttribute();
    FilenameExtAttr = new(NonPagedPool) FILE_NAME();
    VolumeNameAttr = new(NonPagedPool) ResidentAttribute();

    DPRINT1("Finding Attribute...\n");

    VolumeFileRecord->FindFilenameAttribute(FilenameAttrib, FilenameExtAttr, Filename);
    VolumeFileRecord->FindVolumenameAttribute(VolumeNameAttr, VolumeName);

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