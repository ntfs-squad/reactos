/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

FileRecord::FileRecord(_In_ PNTFSVolume Volume)
{
    // Save NTFSVolume pointer.
    this->Volume = Volume;

    // Initialize data buffer and header pointer.
    Data = new(PagedPool, TAG_FILE_RECORD) UCHAR[Volume->MFT->FileRecordSize];
    Header = (PFileRecordHeader)Data;
}

FileRecord::~FileRecord()
{
    if (Data)
        delete Data;
}
