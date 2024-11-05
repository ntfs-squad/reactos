/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "io/ntfsprocs.h"

#define GetUpdateSequenceNumber(Header) \
*((PUSHORT)((ULONG_PTR)Header + Header->Header.UpdateSequenceOffset))

#define GetUpdateSequenceArray(Header) \
(PUSHORT)((ULONG_PTR)Header + Header->Header.UpdateSequenceOffset + sizeof(USHORT))

#define OffsetToNextUSN(Volume) \
(Volume->BytesPerSector - sizeof(USHORT))

/* *** FILE RECORD IMPLEMENTATIONS *** */
FileRecord::FileRecord(_In_ PNTFSVolume Volume,
                       _In_ ULONGLONG FRDiskOffset,
                       _In_ UINT FileRecordSize)
{
    NTSTATUS Status;
    USHORT UpdateSequenceNumber, UpdateSequenceArrayPos;
    PUSHORT UpdateSequenceArray;
    PUSHORT DataPtr;

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

    /* Apply fixup to the file record in memory.
     *
     * Algorithm:
     * 1. Get the (2 byte) update sequence number.
     * 2. Get the update sequence array.
     * 3. Replace the update sequence number at the end of each sector with its
     *    corresponding entry in the update sequence array.
     *
     * Luckily for us, file records are already sector aligned.
     *
     * For more information, see: https://flatcap.github.io/linux-ntfs/ntfs/concepts/fixup.html
     */

    // Get update sequence number
    UpdateSequenceNumber = GetUpdateSequenceNumber(Header);
    UpdateSequenceArray = GetUpdateSequenceArray(Header);

    DataPtr = (PUSHORT)(Data + OffsetToNextUSN(Volume));
    UpdateSequenceArrayPos = 0;

    while (DataPtr < (PUSHORT)((ULONG_PTR)Header + Header->AllocatedSize))
    {
        /* Ensure end of sector is equal to the update sequence number
         * Failing this assertion can be an indicator that this sector is bad.
         * TODO: Add to $BadClus and handle that.
         */
        ASSERT(*DataPtr == UpdateSequenceNumber);

        // Update end of sector from update sequence array
        *DataPtr = UpdateSequenceArray[UpdateSequenceArrayPos];

        // Move on to next sector
        DataPtr = (PUSHORT)((ULONG_PTR)DataPtr + Volume->BytesPerSector);
        UpdateSequenceArrayPos++;
    }
}

FileRecord::~FileRecord()
{
    if (Data)
        delete Data;
}