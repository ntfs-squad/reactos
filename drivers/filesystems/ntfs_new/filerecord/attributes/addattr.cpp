/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

#define UpdatedAttributeSize(OldAttribute, NewDataLength) \
ROUND_UP(OldAttribute->Resident.DataOffset + NewDataLength, 8)

#define NewRecordSize(OldSize, OldAttribute, NewDataLength) \
ROUND_UP(OldSize - OldAttribute->Length + UpdatedAttributeSize(OldAttribute, NewDataLength), 8)

#define NewAttributeEndPtr(OldAttribute, NewDataLength) \
(PUCHAR)OldAttribute + UpdatedAttributeSize(OldAttribute, NewDataLength)

#define EndOfAttributesMarker 0xFFFFFFFF

#define FILE_OFFSET_EOF ~0


// NOTE: ATTRIBUTES ARE ALIGNED TO 8-BYTE BOUNDARIES!!!

NTSTATUS
FileRecord::UpdateAttributeData(_In_     AttributeType Type,
                                _In_opt_ PWSTR AttributeName,
                                _In_     PUCHAR Buffer,
                                _In_     ULONG Length,
                                _In_     PULONGLONG Offset)
{
    PAttribute CurrentAttribute;
    PUCHAR EndOfFileRecord;
    PDataRun NonResidentDataRun, CurrentDR;

    // Find the attribute we need to update.
    CurrentAttribute = GetAttribute(Type, AttributeName);
    if (!CurrentAttribute)
        return STATUS_NOT_FOUND;

    if (*Offset == FILE_OFFSET_EOF)
    {
        *Offset = GetAttributeDataSize(CurrentAttribute);
    }

    /* TODO: Determine if the attribute needs to be made non-resident.
     *
     * Note: MS NTFS doesn't bother shrinking back down to resident once an
     * attribute is made non-resident. I want to try it though.
     */

    if (CurrentAttribute->IsNonResident)
    {
        // The attribute is non-resident.

        // Get the data run for this attribute.
        NonResidentDataRun = FindNonResidentData(CurrentAttribute);

        // Loop through data run to find where we need to update attribute data
        CurrentDR = NonResidentDataRun;

        while(CurrentDR && Length)
        {
            if (*Offset >= (CurrentDR->Length * BytesPerCluster(Volume)))
            {
                // We need to move onto the next data run.
                *Offset -= (CurrentDR->Length * BytesPerCluster(Volume));
            }

            else
            {
                // There is information in this data run we have to edit.
                // WriteDiskUnaligned(Volume->PartDeviceObj,
                //                    Offset + (CurrentDR->LCN * BytesPerCluster(Volume)),
                //                    )

                __debugbreak();
                // Free the data run.
                FreeDataRun(NonResidentDataRun);
                return STATUS_NOT_IMPLEMENTED;
            }

            // Set up next data run.
            CurrentDR = CurrentDR->NextRun;
        }

        // Free the data run.
        FreeDataRun(NonResidentDataRun);
        return STATUS_SUCCESS;
    }

    else
    {
        // The attribute is resident.

        // TODO: Implement offsets
        ASSERT(*Offset == 0);

        // Find the end of the file record.
        EndOfFileRecord = Data + Header->ActualSize;

        // Move the attribute data after the target attribute to where it needs to go
        RtlMoveMemory(NewAttributeEndPtr(CurrentAttribute, Length),
                      (PUCHAR)CurrentAttribute + CurrentAttribute->Length,
                      (EndOfFileRecord - (PUCHAR)CurrentAttribute - CurrentAttribute->Length));

        // Copy the buffer contents into the current attribute
        RtlCopyMemory((PUCHAR)CurrentAttribute + CurrentAttribute->Resident.DataOffset,
                      Buffer,
                      Length);

        // Adjust length of the file record
        Header->ActualSize = NewRecordSize(EndOfFileRecord - (PUCHAR)Data,
                                           CurrentAttribute,
                                           Length);

        // Adjust length of the attribute data
        CurrentAttribute->Length = UpdatedAttributeSize(CurrentAttribute, Length);
        CurrentAttribute->Resident.DataLength = Length;

        return STATUS_SUCCESS;
    }
}

/* Inserts or overwrites an attribute to the file record in memory.
 */
NTSTATUS
FileRecord::InsertAttribute(_In_ PAttribute AttributeToInsert)
{
    /* TODO:
     * Implement support for named attributes.
     * Probably needs to be fixed now with the changes.
     * Note: This function will break attribute pointers.
     */
#if 0
    // THIS FUNCTION IS INCOMPLETE!!!!!!
    PUCHAR FileAttributesPtr;
    ULONG WorkingOffset, AttributesLength, AfterBufferPtr;
    PAttribute CurrentAttribute;
    AttributeType TargetType;

    DPRINT1("Called FileRecord::InsertAttribute()!\n");
    PrintAttributeHeader(AttributeToInsert);
    DPRINT1("Attribute data: \"%s\"\n", (PUCHAR)(AttributeToInsert + AttributeToInsert->Resident.DataOffset));

    TargetType = (AttributeType)AttributeToInsert->AttributeType;
    FileAttributesPtr = Data + Header->AttributeOffset;
    WorkingOffset = 0;
    AttributesLength = Header->ActualSize - Header->AttributeOffset;

    // NOTE!!!!! FILE RECORD REALSIZE IS ROUNDED UP TO NEAREST 8 BYTE BOUNDARY!!!!!
    // N.B. The Real Size will be padded to an 8 byte boundary.

    // Loop through old attributes to copy into the new one.
    while (WorkingOffset < AttributesLength)
    {
        // Test current attribute.
        CurrentAttribute = (PAttribute)(AttributePtr);

        if (CurrentAttribute->AttributeType == EndOfAttributesMarker)
        {
            AttributesLength = WorkingOffset + sizeof(UINT32);
            break;
        }

        if (CurrentAttribute->AttributeType < TargetType)
        {
            // The tested attribute type is less than the target attribute type.
            DPRINT1("Attribute has a type less than the target. Skipping.\n");
            DPRINT1("Attribute type: 0x%02X, Target type: 0x%02X\n", CurrentAttribute->AttributeType, TargetType);
            WorkingOffset += CurrentAttribute->Length;
        }

        else if (CurrentAttribute->AttributeType == TargetType)
        {
            DPRINT1("Attribute type matches!\n");

            // TODO: How do we order attributes by name when they have the same type?
            if (DoesAttributeNameMatch(CurrentAttribute, AttributeToInsert))
            {
                // We will overwrite this attribute.
                DPRINT1("Attribute name matches!\n");

                /* We will overwrite this attribute.
                 * Set the after buffer pointer to the next element.
                 */
                AfterBufferPtr = WorkingOffset + CurrentAttribute->Length;
                break;
            }

            else
            {
                DPRINT1("Inserting attributes with new names is not implemented!\n");
                return STATUS_NOT_IMPLEMENTED;
            }
        }

        else
        {
            // Set the after buffer pointer to this point.
            AfterBufferPtr = WorkingOffset;
            break;
        }
    }

    // The data pointer is pointing to where we need to insert the attribute.
    // Move the attribute data after the attribute we are inserting.
    RtlMoveMemory(FileAttributesPtr + WorkingOffset + AttributeToInsert->Length,
                  FileAttributesPtr + AfterBufferPtr,
                  AttributesLength - WorkingOffset - CurrentAttribute->Length);

    DPRINT1("End marker: 0x%X\n", *((PULONG)(FileAttributesPtr + WorkingOffset + AttributeToInsert->Length)));

    // Copy the new attribute into the attribute block.
    RtlCopyMemory(AttributePtr,
                  AttributeToInsert,
                  AttributeToInsert->Length);

    PrintAttributeHeader((PAttribute)(AttributePtr));
    DPRINT1("Attribute data: \"%s\"\n", (PUCHAR)((AttributePtr) + ((PAttribute)(AttributePtr))->Resident.DataOffset));

    PrintAttributeHeader(AttributeToInsert);
    DPRINT1("Attribute data: \"%s\"\n", (PUCHAR)(AttributeToInsert + AttributeToInsert->Resident.DataOffset));

    WorkingOffset += AttributeToInsert->Length;

    __debugbreak();

    // if (AfterBuffer)
    // {
    //     RtlCopyMemory(AfterBuffer,
    //                   AttributePtr,
    //                   AfterBufferLength);

    //     WorkingOffset += AfterBufferLength;
    // }

    // else
    // {
    //     // Add end marker.
    //     *((PULONG)AttributePtr) = EndOfAttributesMarker;

    //     // The file record size includes the end marker.
    //     WorkingOffset += 4;
    // }

    Header->ActualSize = ROUND_UP(Header->AttributeOffset + WorkingOffset, 8);

    __debugbreak();

    return STATUS_SUCCESS;
#else
    return STATUS_NOT_IMPLEMENTED;
#endif
}
