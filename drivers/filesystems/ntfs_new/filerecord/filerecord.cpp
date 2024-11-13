/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfspch.h"

FileRecord::FileRecord(_In_ PNTFSVolume Volume)
{
    // Save NTFSVolume and MasterFileTable pointers.
    this->Volume = Volume;
    MFT = Volume->MFT;
}

FileRecord::~FileRecord()
{
    if (Data)
        delete Data;
}

NTSTATUS
FileRecord::LoadFileRecordFromDisk(_In_ ULONGLONG FileRecordNumber)
{
    NTSTATUS Status;
    ULONGLONG FileRecordDiskOffset;

    // Initialize data buffer and header pointer.
    if (Data == NULL)
    {
        Data = new(PagedPool) UCHAR[MFT->FileRecordSize];
        Header = (PFileRecordHeader)Data;
    }

    // Get disk offset for the file record.
    Status = MFT->GetFileRecordDiskOffset(FileRecordNumber,
                                          &FileRecordDiskOffset);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get file record disk offset!\n");
        if (Data != NULL)
            delete Data;
        return Status;
    }

    // Read disk for file record contents.
    Status = ReadDisk(Volume->PartDeviceObj,
                      FileRecordDiskOffset,
                      MFT->FileRecordSize,
                      Data);

    // Ensure the file record was read correctly.
    if (!NT_SUCCESS(Status) ||
        !(RtlCompareMemory(Header->Header.TypeID, "FILE", 4) == 4))
    {
        DPRINT1("Failed to get disk contents!\n");
        if (Data != NULL)
            delete Data;
        __debugbreak();
    }

    // Apply fixup
    Status = ApplyFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("File corruption detected!\n");
        delete Data;
        __debugbreak();
    }

    return Status;
}