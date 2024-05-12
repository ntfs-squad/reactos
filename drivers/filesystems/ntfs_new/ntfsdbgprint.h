#include <debug.h>
#include "mft.h"

static inline void PrintFileRecordHeader(FileRecordHeader* FRH)
{
    DPRINT1("MFT Record Number: %ld\n", FRH->MFTRecordNumber);
}

static inline void PrintAttributeHeader(IAttribute* Attr)
{
    DPRINT1("Attribute Type:   0x%X\n", Attr->AttributeType);
    DPRINT1("Length:           %ld\n", Attr->Length);
    DPRINT1("Nonresident Flag: %ld\n", Attr->NonResidentFlag);
    DPRINT1("Name Length:      %ld\n", Attr->NameLength);
    DPRINT1("Name Offset:      %ld\n", Attr->NameOffset);
    DPRINT1("Flags:            0x%X\n", Attr->Flags);
    DPRINT1("Attribute ID:     %ld\n", Attr->AttributeID);
}

static inline void PrintResidentAttributeHeader(ResidentAttribute* Attr)
{
    PrintAttributeHeader(Attr);

    DPRINT1("Attribute Length: %ld\n", Attr->AttributeLength);
    DPRINT1("Attribute Offset: 0x%X\n", Attr->AttributeOffset);
    DPRINT1("IndexedFlag:      %ld\n", Attr->IndexedFlag);
}

static inline void PrintFilenameAttrHeader(FILE_NAME* Attr)
{
    DPRINT1("FileCreation: %ld\n", Attr->FileCreation);
    DPRINT1("FileChanged: %ld\n", Attr->FileChanged);
    DPRINT1("MftChanged: %ld\n", Attr->MftChanged);
    DPRINT1("FileRead: %ld\n", Attr->FileRead);
    DPRINT1("AllocatedSize: %ld\n", Attr->AllocatedSize);
    DPRINT1("RealSize: %ld\n", Attr->RealSize);
    DPRINT1("FilenameChars: %ld\n", Attr->FilenameChars);
}

static inline void PrintVCB(VolumeContextBlock* VCB,
                            char* OEM_ID,
                            UINT16 SECTORS_PER_TRACK,
                            UINT16 NUM_OF_HEADS)
{
    DPRINT1("OEM ID            %s\n", OEM_ID);
    DPRINT1("Bytes per sector  %ld\n", VCB->BytesPerSector);
    DPRINT1("Sectors/cluster   %ld\n", VCB->SectorsPerCluster);
    DPRINT1("Sectors per track %ld\n", SECTORS_PER_TRACK);
    DPRINT1("Number of heads   %ld\n", NUM_OF_HEADS);
    DPRINT1("Sectors in volume %ld\n", VCB->SectorsInVolume);
    DPRINT1("LCN for $MFT      %ld\n", VCB->MFTLCN);
    DPRINT1("LCN for $MFT_MIRR %ld\n", VCB->MFTMirrLCN);
    DPRINT1("Clusters/MFT Rec  %d\n", VCB->ClustersPerFileRecord);
    DPRINT1("Clusters/IndexRec %d\n", VCB->ClustersPerIndexRecord);
    DPRINT1("Serial number     0x%X\n", VCB->SerialNumber);
}