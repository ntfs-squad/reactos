/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

NTSTATUS
FileRecord::WriteRecordToDisk()
{
    NTSTATUS Status;
    ULONGLONG FileRecordDiskOffset;

    // TODO: Add logging

    // Insert the fixup array into the file record in memory.
    Status = CommitFixup();

    // Write to disk.
    Status = MFT->GetFileRecordDiskOffset(Header->MFTRecordNumber,
                                          &FileRecordDiskOffset);

    if (!NT_SUCCESS(Status))
        __debugbreak();

    Status = WriteDisk(Volume->PartDeviceObj,
                       FileRecordDiskOffset,
                       MFT->FileRecordSize,
                       Data);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Status: 0x%X\n", Status);
        __debugbreak();
    }

    // Undo the fixup array to fix the file record in memory.
    Status = ApplyFixup();

    if (!NT_SUCCESS(Status))
        __debugbreak();

    return Status;
}