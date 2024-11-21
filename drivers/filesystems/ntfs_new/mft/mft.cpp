/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

#define InvalidMftZoneReservation(Num) Num < 1 || Num > 4

MasterFileTable::MasterFileTable(_In_ PNTFSVolume TargetVolume,
                                 _In_ UINT64 MFTLCN,
                                 _In_ UINT64 MFTMirrLCN,
                                 _In_ INT8   ClustersPerFileRecord)
{
    NTSTATUS Status;

    Volume = TargetVolume;
    this->MFTLCN = MFTLCN;
    this->MFTMirrLCN = MFTMirrLCN;

    /* Set the file record size, in bytes.
     * If clusters per file record is less than 0, the file record size is 2^(-ClustersPerFileRecord).
     * Otherwise, the file record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector.
     */
    FileRecordSize = ClustersPerFileRecord < 0 ?
                     1 << (-(ClustersPerFileRecord))
                     : ClustersPerFileRecord * BytesPerCluster(Volume);

    /* Get the MftReservationZone registry value.
     * Per Microsoft Learn: This value should be between 1 and 4. 1 is the default.
     * TODO: Actually use it for MFT Zone reservations.
     */
    MftZoneReservation = QueryDwordRegistryValue(L"NtfsMftZoneReservation", 1);
    if (InvalidMftZoneReservation(MftZoneReservation))
        MftZoneReservation = 1;

    // Initialize $MFT
    Status = GetFileRecord(_MFT, &MFTFile);
    if (!NT_SUCCESS(Status))
        DPRINT1("Failed to get $MFT!\n");

    // Initialize $MFTMirr
    Status = GetFileRecord(_MFTMirr, &MFTMirrFile);
    if (!NT_SUCCESS(Status))
        DPRINT1("Failed to get $MFTMirr!\n");
}

NTSTATUS
MasterFileTable::WriteFileRecordToMFT(_In_ PFileRecord File)
{
    NTSTATUS Status;
    ULONGLONG FileRecordDiskOffset;

    // TODO: Add logging here

    // Insert the fixup array into the file record in memory.
    Status = File->CommitFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to commit fixup! (Status: 0x%X)\n", Status);
        return Status;
    }

    // HACK! Use VCN-to-LCN mapping (needed for fragmented MFTs).
    FileRecordDiskOffset = (MFTLCN * BytesPerCluster(Volume))
                           + (File->Header->MFTRecordNumber * FileRecordSize);

    // Write to disk.
    Status = WriteDisk(Volume->PartDeviceObj,
                       FileRecordDiskOffset,
                       FileRecordSize,
                       File->Data);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to write to disk! (Status: 0x%X)\n", Status);
        return Status;
    }

    if (IsFileRecordInMFTMirr(File->Header->MFTRecordNumber))
    {
        /* TODO: Is it even possible that an MFT mirror can become fragmented?
         * Very rarely would it ever be larger than one cluster.
         */
        FileRecordDiskOffset = (MFTMirrLCN * BytesPerCluster(Volume))
                               + (File->Header->MFTRecordNumber * FileRecordSize);

        // Write to disk.
        Status = WriteDisk(Volume->PartDeviceObj,
                           FileRecordDiskOffset,
                           FileRecordSize,
                           File->Data);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to write to MFT Mirror! (Status: 0x%X)\n", Status);
            return Status;
        }
    }

    // Undo the fixup array to fix the file record in memory.
    Status = File->ApplyFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to revert fixup! (Status: 0x%X)\n", Status);
        return Status;
    }

    return Status;
}

NTSTATUS
MasterFileTable::IsFileRecordNumberInUse(_In_  ULONG FileRecordNumber,
                                         _Out_ PBOOLEAN InUse)
{
#if 0
    NTSTATUS Status;
    USHORT Bitmask;
    UCHAR BitmapSection;
    ULONG Size;

    /* This code consistently fails an assertion:
     * .\drivers\storage\class\disk\disk.c(589): residualOffset == 0
     */

    Size = 1;
    Status = MFTFile->CopyData(TypeBitmap,
                               NULL,
                               &BitmapSection,
                               &Size,
                               FileRecordNumber >> 3);

    Bitmask = 1 << (FileRecordNumber % 8);

    if(!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get bitmap!\n");
        return Status;
    }

    *InUse = !!(BitmapSection & Bitmask);
    return STATUS_SUCCESS;
#else
    *InUse = TRUE;
    return STATUS_SUCCESS;
#endif
}