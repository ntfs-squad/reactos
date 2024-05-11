#include <debug.h>
#include "mft.h"

static inline void PrintFileRecordHeader(FileRecordHeader* FRH)
{
    DPRINT1("MFT Record Number: %ld\n", FRH->MFTRecordNumber);
}

static inline void PrintAttributeHeader(IAttribute* Attr)
{
    if (!Attr)
    {
        DPRINT1("attr pointer is NULL!");
        return;
    }

    DPRINT1("Attribute Type:   0x%X\n", Attr->AttributeType);
    DPRINT1("Length:           %ld\n", Attr->Length);
    DPRINT1("Nonresident Flag: %ld\n", Attr->NonResidentFlag);
    DPRINT1("Name Length:      %ld\n", Attr->NameLength);
    DPRINT1("Name Offset:      %ld\n", Attr->NameOffset);
    DPRINT1("Flags:            0x%X\n", Attr->Flags);
    DPRINT1("Attribute ID:     %ld\n", Attr->AttributeID);
}