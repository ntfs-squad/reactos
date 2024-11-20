/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

FileRecord::FileRecord(_In_ PNTFSVolume Volume,
                       _In_ ULONG FileRecordSize)
{
    // Save NTFSVolume pointer.
    this->Volume = Volume;

    // Initialize data buffer and header pointer.
    Data = new(PagedPool, TAG_FILE_RECORD) UCHAR[FileRecordSize];
    Header = (PFileRecordHeader)Data;
}

FileRecord::FileRecord(_In_ PNTFSVolume Volume) : FileRecord(Volume, Volume->MFT->FileRecordSize) { }

FileRecord::~FileRecord()
{
    if (Data)
        delete Data;
}
