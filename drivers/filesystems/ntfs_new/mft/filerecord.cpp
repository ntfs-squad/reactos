#include "io/ntfsprocs.h"

/* *** FILE RECORD IMPLEMENTATIONS *** */

NTSTATUS
FileRecord::LoadData(_In_ PUCHAR FileRecordData,
                     _In_ UINT Length)
{
    PAGED_CODE();
    RtlCopyMemory(Data, FileRecordData, Length);
    return STATUS_SUCCESS;
}

/* Find Attribute Functions */

PIAttribute
FileRecord::FindAttributePointer(_In_ AttributeType Type,
                                 _In_ PCWSTR Name)
{
    ULONG DataPtr;
    PIAttribute TestAttr;

    // Progress data pointer to attribute section.
    DataPtr = Header->AttributeOffset;

    while (DataPtr < Header->ActualSize)
    {
        // Test current attribute
        TestAttr = (PIAttribute)(&Data[DataPtr]);

        if (TestAttr->AttributeType == Type)
        {
            // TODO: Search by attribute name.
            if (Name)
                continue;

            return (PIAttribute)&Data[DataPtr];
        }

        else
        {
            // If the test attribute is 0 length, we are done.
            if (!TestAttr->Length)
                return NULL;

            DataPtr += TestAttr->Length;
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

NTSTATUS
FileRecord::UpdateResidentAttribute(_In_ ResidentAttribute* Attr)
{
    /* TODO:
     * Implement support for named attributes.
     * Note: This function will break attribute pointers.
     * Note: Resident attribute pointer must not point to file record buffer.
     */
    UCHAR OldData[FILE_RECORD_BUFFER_SIZE];
    ULONG DataPtr, OldDataPtr, OldDataLen, AttrType;
    PIAttribute TestAttr;
    BOOLEAN NewAttrWritten = FALSE;

    // Ensure this function wasn't called with a non-resident attribute
#if DBG
    ASSERT(!Attr->NonResidentFlag);
#endif

    DPRINT1("Attempting to update resident attribute...What is in Attr?\n");
    PrintResidentAttributeHeader(Attr);

    // Start pointers at attribute offset.
    DataPtr = Header->AttributeOffset;
    OldDataPtr = Header->AttributeOffset;

    // Store attribute type.
    AttrType = Attr->AttributeType;

    // Back up old data.
    OldDataLen = Header->ActualSize;
    RtlCopyMemory(&OldData,
                  &Data,
                  OldDataLen);

    DPRINT1("Old data copied...\n");
    DPRINT1("\nOldDataPtr: %lu\nOldDataLen: %lu\nDataPtr: %lu\n", OldDataPtr, OldDataLen, DataPtr);

    // Loop through old attributes to copy into the new one.
    while (OldDataPtr < OldDataLen)
    {
        // Test current attribute.
        TestAttr = (PIAttribute)(&OldData[OldDataPtr]);
        // PrintResidentAttributeHeader((ResidentAttribute*)TestAttr);

        // If the test attribute is 0 length, we are done.
        if (!TestAttr->Length)
            break;

        if (TestAttr->AttributeType < AttrType)
        {
            // The tested attribute type is less than the target attribute type.
            DPRINT1("Attribute has a type less than the target. Skipping.\n");
            DPRINT1("Attribute type: 0x%02X, Target type: 0x%02X\n", TestAttr->AttributeType, AttrType);
            DataPtr += TestAttr->Length;
        }

        else
        {
            // The tested attribute type is the same or greater than the target attribute type.
            if (!NewAttrWritten)
            {
                DPRINT1("Attribute has a type greater than or equal to target.\n");
                DPRINT1("The new attribute hasn't been written yet!\n");
                DPRINT1("Clearing old attribute data from this point...\n");

                // Zero out what's left in attribute data.
                RtlZeroMemory(&Data[DataPtr],
                              FILE_RECORD_BUFFER_SIZE - DataPtr);

                DPRINT1("Let's see what the new attribute looks like:\n");
                PrintResidentAttributeHeader((ResidentAttribute*)Attr);

                DPRINT1("Copying new attribute...\n");

                // Copy new attribute into attribute data.
                RtlCopyMemory(&Data[DataPtr],
                              Attr,
                              Attr->Length);

                DPRINT1("Let's check out the attribute header we just copied...\n");
                PrintResidentAttributeHeader((ResidentAttribute*)&Data[DataPtr]);

                // Update pointer.
                DataPtr += Attr->Length;

                DPRINT1("Data is copied!\n");

                if (TestAttr->AttributeType != AttrType)
                {
                    DPRINT1("Copying current attribute because it's not the same as the old one...\n");
                    // If the test attribute type is not the same as an old one, copy it.
                    RtlCopyMemory(&Data[DataPtr],
                                  TestAttr,
                                  TestAttr->Length);

                    PrintResidentAttributeHeader((ResidentAttribute*)&Data[DataPtr]);

                    // Update attribute data pointer.
                    DataPtr += Attr->Length;
                }

                NewAttrWritten = TRUE;
            }

            else
            {
                // The new attribute has already been written
                DPRINT1("Copying an old attribute...\n");
                PrintAttributeHeader(TestAttr);

                if(TestAttr->AttributeType == 0xFFFFFFFF)
                {
                    DPRINT1("Hit end marker. Skipping\n");
                    break;
                }

                // Copy old attribute.
                RtlCopyMemory(&Data[DataPtr],
                              TestAttr,
                              TestAttr->Length);

                DPRINT1("Attribute copied:\n");
                PrintAttributeHeader((PIAttribute)&Data[DataPtr]);

                // Update attribute data pointer.
                DataPtr += TestAttr->Length;
            }
        }

        OldDataPtr += TestAttr->Length;
        DPRINT1("\nOldDataPtr: %lu\nDataPtr: %lu\n", OldDataPtr, DataPtr);
    }

    // Add end marker.
    Data[DataPtr]     = 0xFF;
    Data[DataPtr + 1] = 0xFF;
    Data[DataPtr + 2] = 0xFF;
    Data[DataPtr + 3] = 0xFF;

    // The attribute data pointer went to the end of the attribute data, so it ends there + end marker.
    Header->ActualSize = DataPtr + 4;

    return STATUS_SUCCESS;
}