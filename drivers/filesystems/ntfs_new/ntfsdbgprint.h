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

static inline void PrintFilenameAttrHeader(FileNameEx* Attr)
{
    DPRINT1("FileCreation: %ld\n", Attr->FileCreation);
    DPRINT1("FileChanged: %ld\n", Attr->FileChanged);
    DPRINT1("MftChanged: %ld\n", Attr->MftChanged);
    DPRINT1("FileRead: %ld\n", Attr->FileRead);
    DPRINT1("AllocatedSize: %ld\n", Attr->AllocatedSize);
    DPRINT1("RealSize: %ld\n", Attr->RealSize);
    DPRINT1("FilenameChars: %ld\n", Attr->FilenameChars);
}

static inline void PrintNTFSBootSector(BootSector* PartBootSector)
{
    DPRINT1("OEM ID            %s\n", PartBootSector->OEM_ID);
    DPRINT1("Bytes per sector  %ld\n", PartBootSector->BytesPerSector);
    DPRINT1("Sectors/cluster   %ld\n", PartBootSector->SectorsPerCluster);
    DPRINT1("Sectors per track %ld\n", PartBootSector->SectorsPerTrack);
    DPRINT1("Number of heads   %ld\n", PartBootSector->NumberOfHeads);
    DPRINT1("Sectors in volume %ld\n", PartBootSector->SectorsInVolume);
    DPRINT1("LCN for $MFT      %ld\n", PartBootSector->MFTLCN);
    DPRINT1("LCN for $MFT_MIRR %ld\n", PartBootSector->MFTMirrLCN);
    DPRINT1("Clusters/MFT Rec  %d\n", PartBootSector->ClustersPerFileRecord);
    DPRINT1("Clusters/IndexRec %d\n", PartBootSector->ClustersPerIndexRecord);
    DPRINT1("Serial number     0x%X\n", PartBootSector->SerialNumber);
};
