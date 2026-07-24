/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

MasterFileTable::MasterFileTable(_In_ PVolume TargetVolume,
                                 _In_ UINT64 MFTLCN,
                                 _In_ UINT64 MFTMirrLCN,
                                 _In_ INT8   ClustersPerFileRecord)
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

    /* Cache the $DATA attributes and their decoded run lists so
     * GetFileRecord() doesn't re-decode them on every call.
     */
    if (MFTFile)
    {
        MFTDataAttr = MFTFile->GetAttribute(TypeData, NULL);
        if (MFTDataAttr && MFTDataAttr->IsNonResident)
            MFTDataRuns = MFTFile->FindNonResidentData(MFTDataAttr);
    }

    if (MFTMirrFile)
    {
        MFTMirrDataAttr = MFTMirrFile->GetAttribute(TypeData, NULL);
        if (MFTMirrDataAttr && MFTMirrDataAttr->IsNonResident)
            MFTMirrDataRuns = MFTMirrFile->FindNonResidentData(MFTMirrDataAttr);
    }
}

MasterFileTable::~MasterFileTable()
{
    FreeDataRun(MFTDataRuns);
    FreeDataRun(MFTMirrDataRuns);
    delete MFTFile;
    delete MFTMirrFile;
}

NTSTATUS
MasterFileTable::WriteFileRecordToMFT(_In_ PFileRecord File)
{
    NTSTATUS Status, WriteStatus;
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
    WriteStatus = MFTFile->WriteFileData(TypeData,
                                         NULL,
                                         File->Data,
                                         &FRSize,
                                         &FileRecordOffset,
                                         MFTDataRuns);

    if (!NT_SUCCESS(WriteStatus))
    {
        DPRINT1("Unable to write to disk! (Status: 0x%X)\n", WriteStatus);
    }

    if (NT_SUCCESS(WriteStatus) &&
        IsFileRecordInMFTMirr(File->Header->MFTRecordNumber))
    {
        FRSize = FileRecordSize;

        // Write file record to $MFTMirr.
        WriteStatus = MFTMirrFile->WriteFileData(TypeData,
                                                 NULL,
                                                 File->Data,
                                                 &FRSize,
                                                 &FileRecordOffset,
                                                 MFTMirrDataRuns);

        if (!NT_SUCCESS(WriteStatus))
        {
            DPRINT1("Unable to write to MFT Mirror! (Status: 0x%X)\n",
                    WriteStatus);
        }
    }

    // Undo the fixup array to fix the file record in memory.
    Status = File->ApplyFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to revert fixup! (Status: 0x%X)\n", Status);
        return Status;
    }

    if (!NT_SUCCESS(WriteStatus))
        return WriteStatus;

    // TODO: Update file timestamps

    return Status;
}

NTSTATUS
MasterFileTable::IsFileRecordNumberInUse(_In_  ULONG FileRecordNumber,
                                         _Out_ PBOOLEAN InUse)
{
    NTSTATUS Status;
    UCHAR Bitmask;
    UCHAR BitmapSection;
    ULONG Size;

    if (!MFTFile)
        return STATUS_INVALID_DEVICE_STATE;

    Size = 1;
    Status = MFTFile->CopyData(TypeBitmap,
                               NULL,
                               &BitmapSection,
                               &Size,
                               FileRecordNumber >> 3);

    Bitmask = 1 << (FileRecordNumber % 8);

    if (!NT_SUCCESS(Status) || Size != 0)
    {
        DPRINT1("Failed to get bitmap!\n");
        return NT_SUCCESS(Status) ? STATUS_END_OF_FILE : Status;
    }

    *InUse = !!(BitmapSection & Bitmask);
    return STATUS_SUCCESS;
}
