/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"

// Macros for GetFreeClusters
const UINT8 Zeros[16] = { 4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0 };
#define GetZerosFromNibble(x) Zeros[(UINT8)x]
#define GetZerosFromByte(x) GetZerosFromNibble(x & 0xF) + GetZerosFromNibble(x >> 4)

#define IsBitSet(Byte, Bit) !!((Byte >> Bit) & 1)

NTSTATUS
Volume::GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters)
{
    NTSTATUS Status;
    PFileRecord BitmapFile;
    PAttribute BitmapData;
    ULONG BytesToRead;
    PUCHAR BitmapBuffer;

    // Note: $Bitmap is *always* non-resident on Windows.

    // Get file record for $Bitmap and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _Bitmap,
                                                       &BitmapFile,
                                                       &BitmapData);

    if (!NT_SUCCESS(Status))
        return Status;

    // Get the size of $Bitmap
    BytesToRead = GetAttributeDataSize(BitmapData);

    // Initialize bitmap buffer
    BitmapBuffer = new(NonPagedPool) UCHAR[BytesToRead];

    // Copy attribute data into this buffer.
    BitmapFile->CopyData(BitmapData,
                         BitmapBuffer,
                         &BytesToRead);

    BytesToRead = GetAttributeDataSize(BitmapData) - BytesToRead;
    FreeClusters->QuadPart = 0;

    for (int i = 0; i < BytesToRead; i++)
        FreeClusters->QuadPart += GetZerosFromByte(BitmapBuffer[i]);

    Status = STATUS_SUCCESS;

    // We're done! Time to cleanup.
    delete BitmapBuffer;
    delete BitmapFile;
    return Status;
}
