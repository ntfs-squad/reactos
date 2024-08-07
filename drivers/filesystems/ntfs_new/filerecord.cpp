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

PDataRun
FileRecord::FindNonResidentData(_In_ NonResidentAttribute* DataAttr)
{

    /* TODO:
     * 1. Read Data run. (Attribute: Data)
     * 2. Parse it.
     *   - First byte describes the length of the offset and length`.
     *   - Next section is the length (in clusters).
     *   - Next section is the offset^ (in clusters).
     *   - Next byte is the next data run (if it's fragmented).
     *   - List of data runs is terminated with 0x00.
     * 3. Return head of linked list.
     *
     * ` First nibble is the length. Second nibble is the offset.
     *
     * ^ From 0 for the first, from the previous offset for the rest.
     *   Can be negative.
     */

    char* DataRunPtr;
    PDataRun Head, Temp;
    UINT8 LengthSize, OffsetSize;
    ULONGLONG PreviousOffset;

    // Get pointer to data run.
    DataRunPtr = ((char*)DataAttr) + DataAttr->DataRunsOffset;

    // Populate Head.
    Head = new(NonPagedPool) DataRun();
    Head->NextRun = NULL;

    // Length size is LSB, Offset size is MSB.
    LengthSize = DataRunPtr[0] & 0xF;
    OffsetSize = DataRunPtr[0] >> 4;

    // Copy length and LCN into linked list head.
    RtlCopyMemory(&Head->Length,
                  DataRunPtr + 1,
                  LengthSize);

    RtlCopyMemory(&Head->LCN,
                  DataRunPtr + 1 + LengthSize,
                  OffsetSize);

    // Populate children data runs for head, if available.
    Temp = Head;
    PreviousOffset = Temp->LCN;
    DataRunPtr += (1 + LengthSize + OffsetSize);

    while (DataRunPtr[0])
    {
        // Initialize next item in linked list.
        Temp->NextRun = new(NonPagedPool) DataRun();
        Temp = Temp->NextRun;

        // Get length and offset for current data run.
        LengthSize = DataRunPtr[0] & 0xF;
        OffsetSize = DataRunPtr[0] >> 4;

        // Copy length and LCN into child data run.
        RtlCopyMemory(&Temp->Length,
                      DataRunPtr + 1,
                      LengthSize);

        /* Note: Child LCN's are relative to previous offset. They can be negative.
         * So the real LCN = Previous Offset + LCN.
         * TODO: Make this work with negative offsets.
         */
        RtlCopyMemory(&Temp->LCN,
                      DataRunPtr + 1 + LengthSize,
                      OffsetSize);

        Temp->LCN += PreviousOffset;

        // Move data run pointer to next item.
        DataRunPtr += (1 + LengthSize + OffsetSize);

        // Update Previous Offset
        PreviousOffset = Temp->LCN;
    }

    // The caller is responsible for freeing the linked list.

    return Head;
}