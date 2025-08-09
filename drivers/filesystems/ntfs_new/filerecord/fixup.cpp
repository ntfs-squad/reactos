/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

#define GetUpdateSequenceNumber(Header) \
(PUSHORT)((ULONG_PTR)Header + Header->Header.UpdateSequenceOffset)

#define GetUpdateSequenceArray(Header) \
(PUSHORT)((ULONG_PTR)Header + Header->Header.UpdateSequenceOffset + sizeof(USHORT))

#define OffsetToFirstUSN(Volume) \
(Volume->BytesPerSector - sizeof(USHORT))

#define IncrementUpdateSequenceNumber(Header) \
((*GetUpdateSequenceNumber(Header)) == 0xFFFF) \
? *GetUpdateSequenceNumber(Header) = 0x1 \
: (*GetUpdateSequenceNumber(Header))++

/* NOTE: For more information, see: https://flatcap.github.io/linux-ntfs/ntfs/concepts/fixup.html
 *
 * Abbreviations used
 *     USA: Update Sequence Array
 *     USN: Update Sequence Number
 */

NTSTATUS
FileRecord::CommitFixup()
{
    USHORT UpdateSequenceNumber, USAPos;
    PUSHORT UpdateSequenceArray;
    PUSHORT DataPtr;

    // Increment update sequence number by one (must be non-zero on disk)
    if (*GetUpdateSequenceNumber(Header) == 0)
    {
        *GetUpdateSequenceNumber(Header) = 1;
    }
    else
    {
        IncrementUpdateSequenceNumber(Header);
    }

    // Get update sequence number
    UpdateSequenceNumber = *GetUpdateSequenceNumber(Header);
    UpdateSequenceArray = GetUpdateSequenceArray(Header);

    // Note: We increment USN without journaling here; LFS integration can be added later

    DataPtr = (PUSHORT)(Data + OffsetToFirstUSN(Volume));
    USAPos = 0;

    while (DataPtr < (PUSHORT)((ULONG_PTR)Header + Header->AllocatedSize))
    {
        // Grab the last two bytes of this sector and insert into the USA.
        UpdateSequenceArray[USAPos] = *DataPtr;

        // Set the end of the sector to the USN.
        *DataPtr = UpdateSequenceNumber;

        // Move to the next element.
        DataPtr = (PUSHORT)((ULONG_PTR)DataPtr + Volume->BytesPerSector);
        USAPos++;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
FileRecord::ApplyFixup()
{
    USHORT UpdateSequenceNumber, UpdateSequenceArrayPos;
    PUSHORT UpdateSequenceArray;
    PUSHORT DataPtr;

    /* Apply fixup to the file record in memory.
     *
     * Algorithm:
     * 1. Get the (2 byte) update sequence number.
     * 2. Get the update sequence array.
     * 3. Replace the update sequence number at the end of each sector with its
     *    corresponding entry in the update sequence array.
     *
     * Luckily for us, file records are already sector aligned.
     */

    // Get update sequence number
    UpdateSequenceNumber = *GetUpdateSequenceNumber(Header);
    UpdateSequenceArray = GetUpdateSequenceArray(Header);

    DataPtr = (PUSHORT)(Data + OffsetToFirstUSN(Volume));
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

    return STATUS_SUCCESS;
}
