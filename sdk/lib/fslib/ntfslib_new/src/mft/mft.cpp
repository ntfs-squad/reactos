/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define clamp(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

MasterFileTable::MasterFileTable(
    _In_ PVolume TargetVolume,
    _In_ UINT64 MFTLCN,
    _In_ UINT64 MFTMirrLCN,
    _In_ INT8 ClustersPerFileRecord,
    _In_opt_ PNtfsConfiguration Configuration)
{
    NTSTATUS Status;

    DiskVolume = TargetVolume;
    this->MFTLCN = MFTLCN;
    this->MFTMirrLCN = MFTMirrLCN;

    /* Set the file record size, in bytes.
     * If clusters per file record is less than 0, the file record size is 2^(-ClustersPerFileRecord).
     * Otherwise, the file record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector.
     */
    FileRecordSize = ClustersPerFileRecord < 0 ?
                     1 << (-(ClustersPerFileRecord))
                     : ClustersPerFileRecord * BytesPerCluster(DiskVolume);

    // Initialize $MFT
    Status = GetFileRecord(_MFT, &MFTFile);
    if (!NT_SUCCESS(Status))
        DPRINT1("Failed to get $MFT!\n");

    // Initialize $MFTMirr
    Status = GetFileRecord(_MFTMirr, &MFTMirrFile);
    if (!NT_SUCCESS(Status))
        DPRINT1("Failed to get $MFTMirr!\n");

    MFTZoneEndLCN = CalculateMftZoneEndLCN(Configuration);
}

ULONGLONG
MasterFileTable::CalculateMftZoneEndLCN(
    _In_opt_ PNtfsConfiguration Configuration
)
{
    /* Calculate MFTZoneEndLCN
     *
     * TODO: Should we also walk $BITMAP to figure out if the zone includes used clusters?
     * Or should we assume it doesn't and check if a cluster is used before writing to it?
     *
     * OPTIONS FOR MFT_ZONE_RESERVATION
     *     For Windows 7 and earlier:
     *     0 - 12.5% of the disk (same as 1)
     *     1 - 12.5% of the disk (default)
     *     2 - 25.0% of the disk
     *     3 - 37.5% of the disk
     *     4 - 50.0% of the disk
     *
     *     For Windows 8+:
     *     if (PartitionSize >= 2GB)
     *         MftZoneSize = 200MB * gMftZoneReservation;
     *     else
     *         MftZoneSize = min(MftZoneReservation * PartitionSize / 8, 2 * PartitionSize / 3);
     *         Note: In practice this is the same as legacy scaling for MFT zone reservation values 1-4
     */

    ULONGLONG RequestedMftZoneEndLCN;
    ULONGLONG VolumeSize = DiskVolume->ClustersInVolume * DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector;

    if (!Configuration)
    {
        NtfsConfiguration DefaultConfig= { 0 };

        DefaultConfig.MftZoneReservation = 1;
        DefaultConfig.UseLegacyMftZoneScaling = FALSE;
        return CalculateMftZoneEndLCN(&DefaultConfig);
    }

    // MftZoneReservation values are limited to 1-100
    Configuration->MftZoneReservation = clamp(Configuration->MftZoneReservation, 1, 100);

    // Calculate MFT Zone size, in clusters
    if (VolumeSize < (1ULL << 31 /* 2GB */) ||
        Configuration->UseLegacyMftZoneScaling)
    {
        /* Technically, MFTZoneReservation values greater than 4 are ignored
         * for legacy MFT zone scaling. I don't think that's worth preserving.
         */
        RequestedMftZoneEndLCN = min((DiskVolume->ClustersInVolume * Configuration->MftZoneReservation) >> 3 /* 1/8 * MftZoneReservation of volume */,
                                      (DiskVolume->ClustersInVolume << 1) / 3 /* 2/3 of volume */);
    }

    else
    {
        /* RequestedMftZoneEndLCN = (200MB * MftZoneReservation) / BytesPerCluster */
        RequestedMftZoneEndLCN = ((100ULL << 21) * Configuration->MftZoneReservation) / (DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector);
    }

    // MFT Zone starts at MFTLCN
    RequestedMftZoneEndLCN += MFTLCN;

    // Don't return an MFTZoneEndLCN that's larger than the volume
    return min(RequestedMftZoneEndLCN, DiskVolume->ClustersInVolume);
}

NTSTATUS
MasterFileTable::WriteFileRecordToMFT(_In_ PFileRecord File)
{
    NTSTATUS Status;
    LARGE_INTEGER FileRecordOffset;

    // TODO: Add logging here

    // Insert the fixup array into the file record in memory.
    Status = File->CommitFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to commit fixup! (Status: 0x%X)\n", Status);
        return Status;
    }

    ULONG FRSize = FileRecordSize;
    FileRecordOffset.QuadPart = (File->Header->MFTRecordNumber * FileRecordSize);

    // Write file record to $MFT.
    Status = MFTFile->WriteFileData(TypeData,
                                    NULL,
                                    File->Data,
                                    &FRSize,
                                    &FileRecordOffset);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to write to disk! (Status: 0x%X)\n", Status);
        return Status;
    }

    if (IsFileRecordInMFTMirr(File->Header->MFTRecordNumber))
    {
        // Write file record to $MFTMirr.
        Status = MFTMirrFile->WriteFileData(TypeData,
                                            NULL,
                                            File->Data,
                                            &FRSize,
                                            &FileRecordOffset);

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

    // TODO: Update file timestamps

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
