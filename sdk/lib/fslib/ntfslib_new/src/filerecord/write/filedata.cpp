/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define IsOffsetEndOfFile(Offset) \
Offset->HighPart == -1 && Offset->LowPart == FILE_WRITE_TO_END_OF_FILE

NTSTATUS
FileRecord::WriteFileData(_In_     AttributeType AttrType,
                          _In_opt_ PWSTR StreamName,
                          _In_     PUCHAR Buffer,
                          _Inout_  PULONG Length,
                          _In_     PLARGE_INTEGER Offset)
{
    NTSTATUS Status;
    PAttribute TargetAttribute;

    // If we aren't writing anything, don't write anything.
    if (*Length == 0)
        return STATUS_SUCCESS;

    // Get the target attribute
    TargetAttribute = GetAttribute(AttrType, StreamName);
    if (!TargetAttribute)
    {
        // TODO: We may need to create it in this case, let's see how this API grows up.
        DPRINT1("WriteFileData():GetAttribute() failed!\n");
        return STATUS_NOT_FOUND;
    }

    // Update the offset, if needed.
    if (IsOffsetEndOfFile(Offset))
        Offset->QuadPart = GetAttributeDataSize(TargetAttribute);

    if (!(TargetAttribute->IsNonResident))
    {
        /* Attribute data is resident
         * TODO: Add journaling with LFS
         *       Support offsets
         */

        Status = UpdateResidentData(TargetAttribute,
                                    Buffer,
                                    Length,
                                    Offset->QuadPart);

        if (Status == STATUS_BUFFER_TOO_SMALL)
        {
            /* Unable to write data as resident. Promote to non-resident
             * and write the file data.
             */
            DPRINT1("Promoting to non-resident not supported yet!\n");
            return Status;
        }

        else if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to write file data!\n");
            return Status;
        }

        // Write the file record to disk
        Status = DiskVolume->MFT->WriteFileRecordToMFT(this);
    }

    else
    {
        // Attribute data is nonresident.

        /* Potential improvement: Move attribute data back to resident
         * if small enough. Note: MS NTFS does not appear to do this.
         */

        Status = UpdateNonResidentData(TargetAttribute,
                                       Buffer,
                                       Length,
                                       Offset->QuadPart);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to write to non resident data!\n");
            __debugbreak();
            return Status;
        }
    }

    return Status;
}

NTSTATUS
FileRecord::UpdateNonResidentData(_In_ PAttribute TargetAttribute,
                                  _In_ PUCHAR Buffer,
                                  _In_ PULONG Length,
                                  _In_ ULONGLONG Offset)
{
    NTSTATUS Status;
    ULONGLONG BytesInRun;
    ULONG BytesWritten;
    PDataRun CurrentRun, Head;

    // TODO:
    /* Algorithm (WIP)
     *   - Update clusters marked in use (in file: $Bitmap)
     *   - Update data runs
     *   - Write data to disk
     *       * Note: This action is not journaled by LFS.
     */

    // If this is a resident attribute, fail.
    if (!(TargetAttribute->IsNonResident))
        return STATUS_INVALID_PARAMETER;

    // Do we need to allocate more space?
    if ((Offset + *Length) > TargetAttribute->NonResident.AllocatedSize)
    {
        // Adjust the data runs.
        DPRINT1("Allocating more data space is not implemented!\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    Head = FindNonResidentData(TargetAttribute);
    CurrentRun = Head;
    BytesWritten = 0;

    while (CurrentRun)
    {
        BytesInRun = GetRunSize(CurrentRun);

        if (Offset >= BytesInRun)
        {
            // Skip over this entire data run
            Offset -= BytesInRun;
        }

        else
        {
            // Get data
            Status = DiskVolume->WriteVolume(GetOffset(CurrentRun->LCN) + Offset,
                                             min(*Length, (BytesInRun - Offset)),
                                             Buffer);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to write data contents!\n");
                __debugbreak();
                return Status;
            }

            // Adjust bytes written
            BytesWritten += min(*Length, (BytesInRun - Offset));

            // Are we done writing?
            if (BytesWritten == *Length)
                break;

            // Clear offset
            if (Offset)
                Offset = 0;
        }

        // Set up next data run
        CurrentRun = CurrentRun->NextRun;
    }

    // Free data run
    FreeDataRun(Head);

    // Check to make sure we wrote what was requested
    if (BytesWritten != *Length)
    {
        DPRINT1("Failed to write file data!\n");
        __debugbreak();
        return STATUS_NOT_FOUND;
    }

    // Adjust length for caller
    *Length -= BytesWritten;

    return STATUS_SUCCESS;

}
