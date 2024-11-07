/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "io/ntfsprocs.h"

/* *** FILE RECORD IMPLEMENTATIONS *** */
FileRecord::FileRecord(_In_ PNTFSVolume Volume,
                       _In_ ULONGLONG FRDiskOffset,
                       _In_ UINT FileRecordSize)
{
    NTSTATUS Status;

    DPRINT1("Called FileRecord::FileRecord()!\n");

    // Save NTFSVolume pointer.
    this->Volume = Volume;

    // Initialize data buffer.
    Data = new(PagedPool) UCHAR[FileRecordSize];

    // Get file record from disk.
    Status = ReadDisk(Volume->PartDeviceObj,
                      FRDiskOffset,
                      FileRecordSize,
                      Data);

    // Point header to data
    Header = (PFileRecordHeader)Data;

    // Ensure the file record was read correctly.
    ASSERT(NT_SUCCESS(Status));
    ASSERT(RtlCompareMemory(Header->Header.TypeID, "FILE", 4) == 4);

    // Apply fixup
    ApplyFixup();
}

FileRecord::~FileRecord()
{
    if (Data)
        delete Data;
}