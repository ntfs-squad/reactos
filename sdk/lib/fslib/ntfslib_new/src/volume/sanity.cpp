/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

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
