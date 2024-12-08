/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

NTSTATUS
NTFSVolume::ReadVolume(_In_    ULONGLONG Offset,
                       _In_    ULONG Length,
                       _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status;
    PUCHAR ReadBuffer;
    ULONGLONG SectorAlignedOffset;
    ULONG SectorAlignedLength;

    ASSERT(Length);

    SectorAlignedOffset = Offset - (Offset % BytesPerSector);
    SectorAlignedLength = ALIGN_UP_BY(Length, BytesPerSector);

    if (SectorAlignedOffset == Offset
        && SectorAlignedLength == Length)
    {
        // Read directly to the supplied buffer.
        Status = ReadDisk(PartDeviceObj,
                          SectorAlignedOffset,
                          SectorAlignedLength,
                          Buffer);
    }

    else
    {
        // Read an extra sector if needed.
        if (SectorAlignedOffset != Offset)
            SectorAlignedLength += BytesPerSector;

        // Create the read buffer
        ReadBuffer = new(NonPagedPool) UCHAR[SectorAlignedLength];

        // Fill the read buffer.
        Status = ReadDisk(PartDeviceObj,
                          SectorAlignedOffset,
                          SectorAlignedLength,
                          ReadBuffer);

        if (NT_SUCCESS(Status))
        {
            // Copy the contents we need into the supplied buffer.
            RtlCopyMemory(Buffer,
                          ReadBuffer + (Offset % BytesPerSector),
                          Length);
        }

        // Free read buffer
        delete ReadBuffer;
    }

    return Status;
}

// NTSTATUS
// NTFSVolume::ReadVolumeAsync()
// {
//     return STATUS_NOT_IMPLEMENTED;
// }

NTSTATUS
NTFSVolume::WriteVolume(_In_    ULONGLONG Offset,
                        _In_    ULONG Length,
                        _Inout_ PUCHAR Buffer)
{
    ULONG LengthSectorAligned = ALIGN_DOWN_BY(Length, BytesPerSector);

    if (LengthSectorAligned != Length)
    {
        DPRINT1("LengthSectorAligned != Length (%ld != %ld)\n", LengthSectorAligned, Length);
        __debugbreak();
        return STATUS_NOT_IMPLEMENTED;
    }

    return WriteDisk(PartDeviceObj,
                     Offset,
                     Length,
                     Buffer);
}

// NTSTATUS
// NTFSVolume::WriteVolumeAsync()
// {
//     return STATUS_NOT_IMPLEMENTED;
// }
