#include "io/ntfsprocs.h"

/* Find Attribute Functions */
PAttribute
FileRecord::GetAttribute(_In_ AttributeType Type,
                         _In_ PCWSTR Name)
{
    ULONG DataPtr;
    PAttribute TestAttr;
    UINT NameLength;

    if (Name)
        NameLength = wcslen(Name) * sizeof(WCHAR);

    // Progress data pointer to attribute section.
    DataPtr = Header->AttributeOffset;

    while (DataPtr < Header->ActualSize)
    {
        // Test current attribute
        TestAttr = (PAttribute)(&Data[DataPtr]);

        if (TestAttr->AttributeType == Type)
        {
            // If no name is specified, return the first attribute with the target type.
            if (!Name)
            {
                return (PAttribute)&Data[DataPtr];
            }

            else if ((TestAttr->NameLength) * sizeof(WCHAR) == NameLength &&
                     RtlCompareMemory(GetNamePointer(TestAttr),
                                      Name,
                                      NameLength) == NameLength)
            {
                // We found the attribute!
                return (PAttribute)&Data[DataPtr];
            }

            // This one isn't it. Try again.
        }

        else if (!TestAttr->Length ||
                 TestAttr->AttributeType > Type)
        {
            /* Attributes are stored in ascending order according to its attribute type.
             * If we passed the attribute type, it's not going to show up. Fail early.
             */
            return NULL;
        }

        DataPtr += TestAttr->Length;
    }

    // We went through the whole record and didn't find it.
    return NULL;
}

PDataRun
FileRecord::FindNonResidentData(_In_ PAttribute DataAttr)
{

    /* Algorithm:
     * 1. Read Data run. (Stored in $DATA attribute)
     * 2. Parse it.
     *   - First byte describes the length of the offset and length`.
     *   - Next section is the length (in clusters).
     *   - Next section is the offset^ (in clusters).
     *   - Next byte is the next data run (if it's fragmented).
     *   - List of data runs is terminated with 0x00.
     * 3. Return head of linked list.
     *
     * `: First nibble is the length. Second nibble is the offset.
     *
     * ^: From 0 for the first, from the previous offset for the rest.
     *    Can be negative.
     */

    char* DataRunPtr;
    PDataRun Head, Temp;
    UINT8 LengthSize, OffsetSize;
    ULONGLONG PreviousOffset;

    ASSERT(DataAttr->IsNonResident);

    // Get pointer to data run.
    DataRunPtr = ((char*)DataAttr) + DataAttr->NonResident.DataRunsOffset;

    // Populate Head.
    Head = new(PagedPool) DataRun();
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
        Temp->NextRun = new(PagedPool) DataRun();
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
