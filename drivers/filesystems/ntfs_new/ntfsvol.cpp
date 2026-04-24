/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

NTSTATUS
Volume::ReadVolume(_In_    ULONGLONG Offset,
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

NTSTATUS
Volume::WriteVolume(_In_    ULONGLONG Offset,
                    _In_    ULONG Length,
                    _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status;
    PUCHAR WriteBuffer;
    ULONGLONG SectorAlignedOffset;
    ULONG SectorAlignedLength;

    SectorAlignedOffset = Offset - (Offset % BytesPerSector);
    SectorAlignedLength = ALIGN_UP_BY(Length, BytesPerSector);

    if (SectorAlignedOffset == Offset
        && SectorAlignedLength == Length)
    {
        // Write directly to the disk using the supplied buffer.
        Status = WriteDisk(PartDeviceObj,
                           SectorAlignedOffset,
                           SectorAlignedLength,
                           Buffer);
    }

    else
    {
        // Write an extra sector if needed.
        if (SectorAlignedOffset != Offset)
            SectorAlignedLength += BytesPerSector;

        // Create the write buffer
        WriteBuffer = new(NonPagedPool) UCHAR[SectorAlignedLength];

        // Fill the write buffer with what's on disk.
        Status = ReadDisk(PartDeviceObj,
                          SectorAlignedOffset,
                          SectorAlignedLength,
                          WriteBuffer);

        if (NT_SUCCESS(Status))
        {
            // Copy the buffer contents we want to write into the write buffer.
            RtlCopyMemory(WriteBuffer + (Offset % BytesPerSector),
                          Buffer,
                          Length);

            // Write to the disk.
            Status = WriteDisk(PartDeviceObj,
                               SectorAlignedOffset,
                               SectorAlignedLength,
                               WriteBuffer);
        }

        // Free write buffer
        delete WriteBuffer;
    }

    return Status;
}

void
Volume::SanityCheckBlockIO()
{

    DPRINT1("Running a very close sanity check by reading one block, writing one block and re reading\n\n\n\n");
    UCHAR ReadBuffer[512] = {0};
    UCHAR PostWriteBuffer[512] = {0};
    UCHAR ZeroOutBuffer[512] = {0};

    // Save disk
    ReadVolume(BytesPerSector,
               BytesPerSector,
               ReadBuffer);

    // Erase disk
    WriteVolume(BytesPerSector,
                BytesPerSector,
                ZeroOutBuffer);

    KeStallExecutionProcessor(100);

    // Recover disk
    WriteVolume(BytesPerSector,
                BytesPerSector,
                ReadBuffer);

    // Verify disk
    ReadVolume(BytesPerSector,
               BytesPerSector,
               PostWriteBuffer);

    for (int i = 0; i < 512; i++)
    {
        DPRINT1("ReadBuffer at Location %d, is value: %X\n", i, ReadBuffer[i]);
        DPRINT1("PostWriteBuffer at Location %d, is value: %X\n", i, PostWriteBuffer[i]);

        if (ReadBuffer[i] == PostWriteBuffer[i])
        {
            DPRINT1("Sanity Check passed for iteration %d\n", i);
        }
        else
        {
            __debugbreak();
        }
    }
}

void
Volume::RunSanityChecks()
{
    PAGED_CODE();

    DPRINT1("RunSanityChecks() called\n");
    // SanityCheckBlockIO();

// Wipe drive
#if 0

    WARNING THIS CODE INTENTIONALLY CORRUPTS THE HARDDRIVE
    UCHAR Buffer[512] = {0};

    for(int i = 0; i < 64; i ++)
    {
        DPRINT1("Erasing block\n");
   NTSTATUS Status =  WriteBlock(PartDeviceObj,
          i,
          1,
          512,
          Buffer);
          DPRINT1("Write block Status %X\n", Status);
    }
#endif

//SanityCheck IO calls

}
