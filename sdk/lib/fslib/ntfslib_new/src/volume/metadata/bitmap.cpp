/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

/* Read $Bitmap in fixed-size chunks so we never need one huge allocation
 * for the whole volume bitmap (32MB for a 1TB volume at 4K clusters).
 */
#define BITMAP_CHUNK_SIZE 0x10000 // 64KB, always a multiple of sizeof(ULONG)

NTSTATUS
Volume::GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters)
{
    NTSTATUS Status;
    PFileRecord BitmapFile;
    PAttribute BitmapData;
    PUCHAR BitmapBuffer;
    RTL_BITMAP Bitmap;
    ULONGLONG BitmapOffset;
    ULONG ClustersRemaining, ClustersInChunk, BytesToRead;

    // Note: $Bitmap is *always* non-resident on Windows.

    // Get file record for $Bitmap and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _Bitmap,
                                                       &BitmapFile,
                                                       &BitmapData);

    if (!NT_SUCCESS(Status))
        return Status;

    // Initialize bitmap chunk buffer
    BitmapBuffer = new(NonPagedPool) UCHAR[BITMAP_CHUNK_SIZE];

    FreeClusters->QuadPart = 0;
    BitmapOffset = 0;
    ClustersRemaining = ClustersInVolume;

    /* Count the clear (free) bits chunk by chunk. Only ClustersInVolume
     * bits are valid; the padding bits at the end of $Bitmap are not.
     */
    while (ClustersRemaining != 0)
    {
        ClustersInChunk = min(ClustersRemaining, BITMAP_CHUNK_SIZE * 8);
        BytesToRead = ALIGN_UP_BY((ClustersInChunk + 7) / 8, sizeof(ULONG));

        Status = BitmapFile->CopyData(BitmapData,
                                      BitmapBuffer,
                                      &BytesToRead,
                                      BitmapOffset);
        if (!NT_SUCCESS(Status))
            goto Done;

        RtlInitializeBitMap(&Bitmap, (PULONG)BitmapBuffer, ClustersInChunk);
        FreeClusters->QuadPart += RtlNumberOfClearBits(&Bitmap);

        BitmapOffset += ClustersInChunk / 8;
        ClustersRemaining -= ClustersInChunk;
    }

    Status = STATUS_SUCCESS;

Done:
    // We're done! Time to cleanup.
    delete[] BitmapBuffer;
    delete BitmapFile;
    return Status;
}
