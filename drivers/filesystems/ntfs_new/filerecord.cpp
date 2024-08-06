#include "io/ntfsprocs.h"

/* *** FILE RECORD IMPLEMENTATIONS *** */

NTSTATUS
FileRecord::LoadData(_In_ PUCHAR FileRecordData,
                     _In_ unsigned Length)
{
    PAGED_CODE();
    AttrLength = Length - sizeof(FileRecordHeader);

    RtlCopyMemory(&Header, &FileRecordData, sizeof(FileRecordHeader));
    RtlCopyMemory(&AttrData,
                  &FileRecordData[Header->AttributeOffset],
                  AttrLength);

    // PrintFileRecordHeader(Header);
    return STATUS_SUCCESS;
}

/* Find Attribute Functions */

PIAttribute
FileRecord::FindAttributePointer(_In_ AttributeType Type,
                                 _In_ PCWSTR Name)
{
    ULONG AttrDataPointer = 0;
    PIAttribute TestAttr;

    DPRINT1("Attribute Length: %d\n", AttrLength);

    while (AttrDataPointer < AttrLength)
    {
        // Test current attribute
        TestAttr = (PIAttribute)(&AttrData[AttrDataPointer]);
        PrintAttributeHeader(TestAttr);

        if (TestAttr->AttributeType == Type)
        {
            DPRINT1("Found attribute type!\n");

            // TODO: Search by attribute name.
            if (Name)
                continue;

            return (PIAttribute)&AttrData[AttrDataPointer];
        }

        else
        {
            // If the test attribute is 0 length, we are done.
            if (!TestAttr->Length)
                return NULL;

            AttrDataPointer += TestAttr->Length;

            DPRINT1("Trying next attribute!\n");
        }
    }

    return NULL;
}
