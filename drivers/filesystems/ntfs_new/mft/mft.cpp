/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

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

NTSTATUS
MasterFileTable::AllocateFreeFileRecord(_Out_ PULONG FileRecordNumber)
{
    /* Temporary allocator: linear probe for a free record number.
     * TODO: Read and update $MFT bitmap properly and journal with LFS.
     */
    NTSTATUS Status;
    ULONG probe = 24; // skip first metadata records

    // Query $MFT::$BITMAP attribute
    PFileRecord BitmapOwner;
    PAttribute  BitmapAttr;
    Status = GetFileAttributeFromFileRecordNumber(TypeBitmap, NULL, _MFT, &BitmapOwner, &BitmapAttr);
    if (!NT_SUCCESS(Status) || !BitmapAttr)
    {
        // Fallback: linear probe without bitmap
        BOOLEAN inUse;
        for (;; ++probe)
        {
            Status = IsFileRecordNumberInUse(probe, &inUse);
            if (!NT_SUCCESS(Status)) return Status;
            if (!inUse) { *FileRecordNumber = probe; return STATUS_SUCCESS; }
            if (probe > 0x00FFFFFF) return STATUS_DISK_FULL;
        }
    }

    // Read bitmap into memory
    ULONG bytesToRead = (ULONG)((probe >> 3) + 0x1000); // read a chunk
    PUCHAR map = (PUCHAR)ExAllocatePoolWithTag(PagedPool, bytesToRead, TAG_MFT);
    if (!map) { delete BitmapOwner; return STATUS_INSUFFICIENT_RESOURCES; }
    RtlZeroMemory(map, bytesToRead);
    Status = MFTFile->CopyData(TypeBitmap, NULL, map, &bytesToRead, 0);
    if (!NT_SUCCESS(Status)) { ExFreePoolWithTag(map, TAG_MFT); delete BitmapOwner; return Status; }

    // Scan for a zero bit
    ULONG bitIndex = 24; // skip system
    for (;; ++bitIndex)
    {
        ULONG byteIndex = bitIndex >> 3;
        if (byteIndex >= bytesToRead)
        {
            // TODO: grow read range; for now, fail
            ExFreePoolWithTag(map, TAG_MFT);
            delete BitmapOwner;
            return STATUS_DISK_FULL;
        }
        UCHAR b = map[byteIndex];
        if (!(b & (1u << (bitIndex & 7))))
        {
            *FileRecordNumber = bitIndex;
            break;
        }
    }

    // Mark bit as used and write back bitmap (simplified; no journaling, no cache)
    ULONG setByte = (*FileRecordNumber) >> 3;
    UCHAR mask = (UCHAR)(1u << ((*FileRecordNumber) & 7));
    map[setByte] |= mask;
    ULONG writeLen = bytesToRead;
    LARGE_INTEGER zeroOffset; zeroOffset.QuadPart = 0;
    Status = MFTFile->WriteFileData(TypeBitmap, NULL, map, &writeLen, &zeroOffset);

    ExFreePoolWithTag(map, TAG_MFT);
    delete BitmapOwner;
    return Status;
}