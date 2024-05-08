#pragma once
#include "contextblocks.h"

class NtfsPartition
{
public:
    VolumeContextBlock* VCB = new(NonPagedPool) VolumeContextBlock();

    NtfsPartition(PDEVICE_OBJECT DeviceToMount);
    ~NtfsPartition();
    void CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject);
    NTSTATUS NtfsPartition::DumpBlocks(_Inout_ PUCHAR Buffer,
                                       _In_    ULONG Lba,
                                       _In_    ULONG LbaCount);
    void RunSanityChecks();
    PFILE_OBJECT PubFileObject;
    PDEVICE_OBJECT PartDeviceObj;

private:
    PVPB VolumeParameterBlock;



    /* Used for debug print */
    char*    OEM_ID;
	UCHAR    MEDIA_DESCRIPTOR;
	UINT16   SECTORS_PER_TRACK;
	UINT16   NUM_OF_HEADS;
};