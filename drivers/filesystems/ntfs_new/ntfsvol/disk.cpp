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
    PUCHAR SectorAlignmentBuffer = NULL;
    USHORT RaggedEdgeSize = 0;
    ULONG LengthSectorAligned = ALIGN_DOWN_BY(Length, BytesPerSector);

    ASSERT(Length);

    if (LengthSectorAligned != Length)
    {
        DPRINT1("LengthSectorAligned != Length (%ld != %ld)\n", LengthSectorAligned, Length);
        RaggedEdgeSize = Length - LengthSectorAligned;
        SectorAlignmentBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, BytesPerSector, TAG_NTFS);
    }

    if (LengthSectorAligned)
    {
        // Note: LengthSectorAligned will equal 0 if we only need to read 1 sector.
        Status = ReadDisk(PartDeviceObj,
                          Offset,
                          LengthSectorAligned,
                          Buffer);

        // TODO: Replace with DPRINT and fail
        ASSERT(NT_SUCCESS(Status));
    }

    if (SectorAlignmentBuffer)
    {
        // Get the last sector of data
        Status = ReadDisk(PartDeviceObj,
                          (Offset + LengthSectorAligned),
                          BytesPerSector,
                          SectorAlignmentBuffer);

        // TODO: Replace with DPRINT and fail
        ASSERT(NT_SUCCESS(Status));

        // Copy what we need into the buffer
        RtlCopyMemory(Buffer + LengthSectorAligned,
                      SectorAlignmentBuffer,
                      RaggedEdgeSize);

        // Free page alignment buffer
        delete SectorAlignmentBuffer;
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
