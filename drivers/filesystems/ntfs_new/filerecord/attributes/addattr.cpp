/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfspch.h"

#define UpdatedAttributeSize(OldAttribute, NewDataLength) \
ROUND_UP(OldAttribute->Resident.DataOffset + NewDataLength, 8)

#define NewAttributeEndPtr(OldAttribute, NewDataLength) \
(PUCHAR)OldAttribute + UpdatedAttributeSize(OldAttribute, NewDataLength)

#define NewRecordSize(OldSize, OldAttribute, NewDataLength) \
ROUND_UP(OldSize - OldAttribute->Length + UpdatedAttributeSize(OldAttribute, NewDataLength), 8)

#define AttributePtr FileAttributesPtr + WorkingOffset

#define EndOfAttributesMarker 0xFFFFFFFF

NTSTATUS
FileRecord::UpdateAttributeData(_In_     AttributeType Type,
                                _In_opt_ PWSTR AttributeName,
                                _In_     PUCHAR Buffer,
                                _In_     ULONG Length,
                                _In_     ULONG Offset)
{
    PAttribute CurrentAttribute;
    PUCHAR EndOfFileRecord;

    // NOTE: ATTRIBUTES ARE ALIGNED TO 8-BYTE BOUNDARIES!!!

    // Eventually I want this to work with offsets
    // but it doesn't work yet.
    ASSERT(Offset == 0);

    // Find the attribute we need to update.
    CurrentAttribute = GetAttribute(Type, AttributeName);
    if (!CurrentAttribute)
        return STATUS_NOT_FOUND;

    // Eventually I want this to work with nonresident attributes
    // but it doesn't work yet.
    ASSERT(!CurrentAttribute->IsNonResident);

    // Find the real end of the file record.
    EndOfFileRecord = (PUCHAR)GetAttribute(TypeAttributeEndMarker, NULL);
    if (!EndOfFileRecord)
        return STATUS_FILE_CORRUPT_ERROR;
    EndOfFileRecord += sizeof(UINT32);

    // Move the attributes after the found attribute.
    DPRINT1("Moving from: 0x%X\n", (PUCHAR)CurrentAttribute + CurrentAttribute->Length);
    DPRINT1("Moving to: 0x%X\n", NewAttributeEndPtr(CurrentAttribute, Length));
    DPRINT1("Moving %ld bytes.\n", NewAttributeEndPtr(CurrentAttribute, Length) - ((PUCHAR)CurrentAttribute + CurrentAttribute->Length));

    DPRINT1("End Of File Record: 0x%X\n", EndOfFileRecord);
    DPRINT1("Current Attribute: 0x%X\n", CurrentAttribute);
    DPRINT1("Current Attribute Length: %ld\n", CurrentAttribute->Length);

    DPRINT1("Length: %ld\n", EndOfFileRecord - (PUCHAR)CurrentAttribute - CurrentAttribute->Length);

    // __debugbreak();

    RtlMoveMemory(NewAttributeEndPtr(CurrentAttribute, Length),
                  (PUCHAR)CurrentAttribute + CurrentAttribute->Length,
                  (EndOfFileRecord - (PUCHAR)CurrentAttribute - CurrentAttribute->Length));

    DPRINT1("Copying to: 0x%X\n", ((PUCHAR)CurrentAttribute + CurrentAttribute->Resident.DataOffset));
    DPRINT1("Copying from: 0x%X\n", Buffer);
    DPRINT1("Length:\n", Length);

    DPRINT1("Buffer Contents: \"%s\"\n", Buffer);

    // __debugbreak();

    // Copy the buffer contents into the current attribute.
    RtlCopyMemory((PUCHAR)CurrentAttribute + CurrentAttribute->Resident.DataOffset,
                  Buffer,
                  Length);

    DPRINT1("Old File Record Size: %ld\n", Header->ActualSize);
    DPRINT1("New File Record Size: %ld\n", NewRecordSize(EndOfFileRecord - (PUCHAR)Data, CurrentAttribute, Length));

    // __debugbreak();

    // Adjust length of the file record
    Header->ActualSize = NewRecordSize(EndOfFileRecord - (PUCHAR)Data,
                                       CurrentAttribute,
                                       Length);

    DPRINT1("Old Attribute Length: %ld\n", CurrentAttribute->Length);
    DPRINT1("New Attribute Length: %ld\n", UpdatedAttributeSize(CurrentAttribute, Length));
    DPRINT1("Old Attribute Data Length: %ld\n", CurrentAttribute->Resident.DataLength);
    DPRINT1("New Attribute Data Length: %ld\n", Length);

    // __debugbreak();

    // Adjust length of the attribute data
    CurrentAttribute->Length = ROUND_UP(UpdatedAttributeSize(CurrentAttribute, Length), 8);
    CurrentAttribute->Resident.DataLength = Length;

    return STATUS_SUCCESS;
}

// #define DoesAttributeNameMatch(Attribute1, Attribute2) \
// Attribute1->NameLength == Attribute2->NameLength \
// && (Attribute1->NameLength == 0 \
//     || RtlCompareMemory(Attribute1 + Attribute1->NameOffset, \
//                         Attribute2 + Attribute2->NameOffset, \
//                         Attribute1->NameLength) == Attribute1->NameLength)

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
