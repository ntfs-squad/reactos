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

#define NewRecordSize(OldAttribute, NewDataLength) \
Header->ActualSize - OldAttribute->Length + UpdatedAttributeSize(OldAttribute, NewDataLength)

#define MustPromoteToNonResident(TargetAttribute, NewDataLength) \
(NewRecordSize(TargetAttribute, NewDataLength) > (Header->AllocatedSize))

#define CanDemoteAttribute(OldAttribute, NewDataLength) \
!MustPromoteAttribute(OldAttribute, NewDataLength)

#define NewAttributeEndPtr(OldAttribute, NewDataLength) \
(PUCHAR)OldAttribute + UpdatedAttributeSize(OldAttribute, NewDataLength)

// NOTE: ATTRIBUTES ARE ALIGNED TO 8-BYTE BOUNDARIES!!!

NTSTATUS
FileRecord::UpdateResidentData(_In_ PAttribute TargetAttribute,
                               _In_ PUCHAR Buffer,
                               _In_ PULONG Length,
                               _In_ ULONGLONG  Offset)
{
    PUCHAR EndOfFileRecord;

    /* TODO: Determine if the attribute needs to be made non-resident.
     *
     * Note: MS NTFS doesn't bother shrinking back down to resident once an
     * attribute is made non-resident. I want to try it though.
     */

    // If this is a nonresident attribute, fail.
    if (TargetAttribute->IsNonResident)
        return STATUS_INVALID_PARAMETER;

    /* If there is not enough room for the new attribute data, return
     * STATUS_BUFFER_TOO_SMALL.
     */
    if (MustPromoteToNonResident(TargetAttribute, (Offset + *Length)))
        return STATUS_BUFFER_TOO_SMALL;

    // TODO: Implement offsets
    ASSERT(Offset == 0);

    // Find the end of the file record.
    EndOfFileRecord = Data + Header->ActualSize;

    // Adjust length of the file record
    Header->ActualSize = NewRecordSize(TargetAttribute,
                                       *Length);

    // Adjust length of the attribute data
    TargetAttribute->Length = UpdatedAttributeSize(TargetAttribute, *Length);
    TargetAttribute->Resident.DataLength = *Length;

    // Move the attribute data after the target attribute to where it needs to go
    RtlMoveMemory(NewAttributeEndPtr(TargetAttribute, *Length),
                  (PUCHAR)TargetAttribute + TargetAttribute->Length,
                  (EndOfFileRecord - (PUCHAR)TargetAttribute - TargetAttribute->Length));

    // Copy the buffer contents into the current attribute
    RtlCopyMemory((PUCHAR)TargetAttribute + TargetAttribute->Resident.DataOffset,
                  Buffer,
                  *Length);

    return STATUS_SUCCESS;
}
