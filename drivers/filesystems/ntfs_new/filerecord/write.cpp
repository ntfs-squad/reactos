#include "io/ntfsprocs.h"

NTSTATUS
FileRecord::UpdateResidentAttribute(_In_ PAttribute Attr)
{
    /* TODO:
     * Implement support for named attributes.
     * Note: This function will break attribute pointers.
     * Note: Resident attribute pointer must not point to file record buffer.
     */
    UCHAR OldData[FILE_RECORD_BUFFER_SIZE];
    ULONG DataPtr, OldDataPtr, OldDataLen, AttrType;
    PAttribute TestAttr;
    BOOLEAN NewAttrWritten = FALSE;

    // Ensure this function wasn't called with a non-resident attribute
    ASSERT(!(Attr->IsNonResident));

    DPRINT1("Attempting to update resident attribute...What is in Attr?\n");
    PrintAttributeHeader(Attr);

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
        TestAttr = (PAttribute)(&OldData[OldDataPtr]);
        // PrintAttributeHeader(TestAttr);

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
                PrintAttributeHeader(Attr);

                DPRINT1("Copying new attribute...\n");

                // Copy new attribute into attribute data.
                RtlCopyMemory(&Data[DataPtr],
                              Attr,
                              Attr->Length);

                DPRINT1("Let's check out the attribute header we just copied...\n");
                PrintAttributeHeader((PAttribute)&Data[DataPtr]);

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

                    PrintAttributeHeader((PAttribute)&Data[DataPtr]);

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
                PrintAttributeHeader((PAttribute)&Data[DataPtr]);

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

NTSTATUS
FileRecord::SetAttribute(_In_ PAttribute Attr)
{
    if (!(Attr->IsNonResident))
    {
        // Attribute is resident.
    }

    else
    {
        // Attribute is non-resident.
    }

    __debugbreak();
    return STATUS_NOT_IMPLEMENTED;
}