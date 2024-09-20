#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

extern NPAGED_LOOKASIDE_LIST FileCBLookasideList;
//TODO:
extern PDRIVER_OBJECT NtfsDriverObject;

static
NTSTATUS
NtfsGetVolumeInformation(PDEVICE_OBJECT DeviceObject,
                         PFILE_FS_VOLUME_INFORMATION Buffer,
                         PULONG Length)
{
    size_t VolumeInfoSize = sizeof(FILE_FS_VOLUME_INFORMATION);

    if (*Length < VolumeInfoSize + DeviceObject->Vpb->VolumeLabelLength)
        return STATUS_BUFFER_TOO_SMALL;

    Buffer->VolumeSerialNumber = DeviceObject->Vpb->SerialNumber;
    Buffer->VolumeLabelLength = DeviceObject->Vpb->VolumeLabelLength;
    RtlCopyMemory(Buffer->VolumeLabel,
                  DeviceObject->Vpb->VolumeLabel,
                  DeviceObject->Vpb->VolumeLabelLength);

    // TODO: Fix this
    Buffer->VolumeCreationTime.QuadPart = 0;
    Buffer->SupportsObjects = FALSE;

    // TODO: Investigate. Should we be returning the bytes written instead?
    *Length -= VolumeInfoSize + DeviceObject->Vpb->VolumeLabelLength;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NtfsGetSizeInfo(PDEVICE_OBJECT DeviceObject,
                PFILE_FS_SIZE_INFORMATION Buffer,
                PULONG Length)
{
    PNtfsPartition Partition;

    if (*Length < sizeof(FILE_FS_SIZE_INFORMATION))
        return STATUS_BUFFER_OVERFLOW;

    Partition = ((PVolumeContextBlock)(DeviceObject->DeviceExtension))->PartitionObj;

    if (!Partition)
        return STATUS_INSUFFICIENT_RESOURCES;

    Partition->GetFreeClusters(&Buffer->AvailableAllocationUnits); // Set # of free clusters
    Buffer->TotalAllocationUnits.QuadPart = Partition->ClustersInVolume; //# of total clusters
    Buffer->SectorsPerAllocationUnit = Partition->SectorsPerCluster; // Sectors per cluster
    Buffer->BytesPerSector = Partition->BytesPerSector; // Bytes per sector

    *Length -= sizeof(FILE_FS_SIZE_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
NtfsGetAttributeInfo(PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
                     PULONG Length)
{
    size_t BytesToWrite = sizeof(FILE_FS_ATTRIBUTE_INFORMATION) + 8;

    if (*Length < BytesToWrite)
        return STATUS_BUFFER_TOO_SMALL;

    Buffer->FileSystemAttributes = FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK |
                                   FILE_READ_ONLY_VOLUME;
    Buffer->MaximumComponentNameLength = 255;
    Buffer->FileSystemNameLength = 8;

    RtlCopyMemory(Buffer->FileSystemName, L"NTFS", 8);

    *Length -= BytesToWrite;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NtfsSetVolumeLabel(_In_ PDEVICE_OBJECT DeviceObject,
                   _In_ PFILE_FS_LABEL_INFORMATION NewLabel,
                   _In_ PULONG Length)
{
    NTSTATUS Status;
    PNtfsPartition Partition;

    Partition = ((PVolumeContextBlock)(DeviceObject->DeviceExtension))->PartitionObj;

    if (!Partition || !NewLabel)
        return STATUS_INSUFFICIENT_RESOURCES;

    // TODO: Implement.
    DPRINT1("Old volume label: \"%S\". Length: %ld\n", DeviceObject->Vpb->VolumeLabel, DeviceObject->Vpb->VolumeLabelLength);
    DPRINT1("Requested new volume label: \"%S\". Length: %ld\n", NewLabel->VolumeLabel, NewLabel->VolumeLabelLength);

    Partition->SetVolumeLabel(NewLabel->VolumeLabel, NewLabel->VolumeLabelLength);

    // Re-read volume label.
    Status = Partition->GetVolumeLabel(DeviceObject->Vpb->VolumeLabel,
                                       &DeviceObject->Vpb->VolumeLabelLength);

    DPRINT1("Volume Label updated!\n");
    DPRINT1("Label: \"%S\", Length: %ld\n",
            DeviceObject->Vpb->VolumeLabel,
            DeviceObject->Vpb->VolumeLabelLength);

    return Status;
}