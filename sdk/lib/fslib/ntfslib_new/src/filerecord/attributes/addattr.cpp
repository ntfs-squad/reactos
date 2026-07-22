/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define UpdatedAttributeSize(OldAttribute, NewDataLength) \
ALIGN_UP_BY(OldAttribute->Resident.DataOffset + NewDataLength, 8)

#define NewRecordSize(OldAttribute, NewDataLength) \
Header->ActualSize - OldAttribute->Length + UpdatedAttributeSize(OldAttribute, NewDataLength)

#define MustPromoteToNonResident(TargetAttribute, NewDataLength) \
(NewRecordSize(TargetAttribute, NewDataLength) > (Header->AllocatedSize))

// NOTE: ATTRIBUTES ARE ALIGNED TO 8-BYTE BOUNDARIES!!!

NTSTATUS
FileRecord::UpdateResidentData(_In_ PAttribute TargetAttribute,
                               _In_ PUCHAR Buffer,
                               _In_ PULONG Length,
                               _In_ ULONGLONG  Offset)
{
    PUCHAR EndOfFileRecord;
    PUCHAR DataStart;
    ULONGLONG oldDataLen;
    ULONGLONG newDataEnd;
    ULONG newAttrSizeAligned;
    ULONG delta;

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
    newDataEnd = Offset + *Length;
    if (MustPromoteToNonResident(TargetAttribute, (ULONG)newDataEnd))
        return STATUS_BUFFER_TOO_SMALL;

    // Compute pointers and sizes
    DataStart = (PUCHAR)TargetAttribute + TargetAttribute->Resident.DataOffset;
    oldDataLen = TargetAttribute->Resident.DataLength;

    // Find the end of the file record.
    EndOfFileRecord = Data + Header->ActualSize;

    // Grow attribute if needed (taking 8-byte alignment into account)
    newAttrSizeAligned = UpdatedAttributeSize(TargetAttribute, (ULONG)newDataEnd);
    if (newAttrSizeAligned > TargetAttribute->Length)
    {
        delta = newAttrSizeAligned - TargetAttribute->Length;
        // Move the attribute tail forward to make room for the growth
        RtlMoveMemory((PUCHAR)TargetAttribute + TargetAttribute->Length + delta,
                      (PUCHAR)TargetAttribute + TargetAttribute->Length,
                      (SIZE_T)(EndOfFileRecord - ((PUCHAR)TargetAttribute + TargetAttribute->Length)));

        // Update record and attribute sizes
        Header->ActualSize += delta;
        TargetAttribute->Length = newAttrSizeAligned;
    }

    // Zero-fill any gap between old end and the write offset
    if (Offset > oldDataLen)
        RtlZeroMemory(DataStart + oldDataLen, (SIZE_T)(Offset - oldDataLen));

    // The data length only increases when we write past the old end.
    if (newDataEnd > oldDataLen)
        TargetAttribute->Resident.DataLength = (ULONG)newDataEnd;

    // Copy the buffer contents at the requested offset
    RtlCopyMemory(DataStart + Offset, Buffer, *Length);

    return STATUS_SUCCESS;
}
