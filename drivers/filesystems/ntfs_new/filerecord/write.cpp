/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "io/ntfsprocs.h"

#define CalculateNewRecordSize(OldRecordSize, OldAttribute, NewDataLength) \
((OldRecordSize) - (OldAttribute->Resident.DataLength) + (NewDataLength))

NTSTATUS
FileRecord::WriteData(_In_ AttributeType Type,
                      _In_ PCWSTR Name,
                      _In_ PUCHAR Buffer,
                      _Inout_ PULONG Length,
                      _In_ BOOLEAN WriteToEndOfFile,
                      _In_ ULONGLONG Offset)
{
    PAttribute Attr = GetAttribute(Type, Name);

    if (Attr)
        return WriteData(Attr, Buffer, Length, WriteToEndOfFile, Offset);

    return STATUS_NOT_FOUND;
}

NTSTATUS
FileRecord::WriteData(_In_ PAttribute Attr,
                      _In_ PUCHAR Buffer,
                      _Inout_ PULONG Length,
                      _In_ BOOLEAN WriteToEndOfFile,
                      _In_ ULONGLONG Offset)
{
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Offset);

    DPRINT1("Called FileRecord::WriteData()!\n");

    if (!(Attr->IsNonResident))
    {
        // Attribute is resident.
        if (CalculateNewRecordSize(this->Header->ActualSize, Attr, *Length) > (this->Header->AllocatedSize))
        {
            //TODO: Promote to non-resident.
            DPRINT1("Unable to promote attribute to non-resident!\n");
            return STATUS_NOT_IMPLEMENTED;
        }

        DPRINT1("Updating file record...\n");
        DPRINT1("Windows may not like the lack of journaling.\n");

        return STATUS_NOT_IMPLEMENTED;
    }

    else
    {
        // Attribute is non-resident.
        DPRINT1("We don't support writing to nonresident files yet\n");
        __debugbreak();
        return STATUS_NOT_IMPLEMENTED;
    }
}

// NTSTATUS
// FileRecord::WriteRecordToDisk()
// {
//     // Insert the fixup array into the file record in memory.
//     // Write to disk.
//     // Undo the fixup array to fix file record in memory.
// }

NTSTATUS
FileRecord::UpdateResidentAttribute(_In_ PAttribute Attr)
{
    /* TODO:
     * Implement support for named attributes.
     * Probably needs to be fixed now with the changes.
     * Note: This function will break attribute pointers.
     * Note: Resident attribute pointer must not point to file record buffer.
     */
    PUCHAR OldData;
    ULONG DataPtr, OldDataPtr, OldDataLen, AttrType;
    PAttribute TestAttr;
    BOOLEAN NewAttrWritten = FALSE;

    DPRINT1("Called FileRecord::UpdateResidentAttribute()!\n");

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
    OldData = new(PagedPool) UCHAR[OldDataLen];
    RtlCopyMemory(OldData,
                  Data,
                  OldDataLen);

    DPRINT1("Old data copied...\n");
    DPRINT1("\nOldDataPtr: %lu\nOldDataLen: %lu\nDataPtr: %lu\n", OldDataPtr, OldDataLen, DataPtr);

    // Loop through old attributes to copy into the new one.
    while (OldDataPtr < OldDataLen)
    {
        // Test current attribute.
        TestAttr = (PAttribute)(OldData[OldDataPtr]);
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
                              OldDataLen - DataPtr);

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
