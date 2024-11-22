/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

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
        Status = Volume->MFT->WriteFileRecordToMFT(this);
    }

    else
    {
        // Attribute data is nonresident.

        DPRINT1("Non-resident writes are not supported yet!\n");
        return STATUS_NOT_IMPLEMENTED;

        /* Potential improvement: Move attribute data back to resident
         * if small enough. Note: MS NTFS does not appear to do this.
         */

        // Update clusters marked in use (in file: $Bitmap)

        // Update data run

        /* Write data to disk
         * Note: This action is not journaled by LFS.
         */
    }

    return Status;
}
