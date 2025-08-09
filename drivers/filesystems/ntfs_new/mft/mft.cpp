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
        // Reset length for second write
        FRSize = FileRecordSize;
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
    NTSTATUS Status;
    PFileRecord BitmapOwner;
    PAttribute  BitmapAttr;

    Status = GetFileAttributeFromFileRecordNumber(TypeBitmap, NULL, _MFT, &BitmapOwner, &BitmapAttr);
    if (!NT_SUCCESS(Status))
        return Status;

    ULONGLONG dataSize = GetAttributeDataSize(BitmapAttr);
    ULONGLONG byteOffset = (ULONGLONG)(FileRecordNumber >> 3);
    if (byteOffset >= dataSize)
    {
        delete BitmapOwner;
        return STATUS_INVALID_PARAMETER;
    }

    UCHAR b = 0;
    ULONG len = 1;
    Status = MFTFile->CopyData(TypeBitmap, NULL, &b, &len, byteOffset);
    if (!NT_SUCCESS(Status))
    {
        delete BitmapOwner;
        return Status;
    }

    UCHAR mask = (UCHAR)(1u << (FileRecordNumber & 7));
    *InUse = !!(b & mask);
    delete BitmapOwner;
    return STATUS_SUCCESS;
}

NTSTATUS
MasterFileTable::AllocateFreeFileRecord(_Out_ PULONG FileRecordNumber)
{
    /* Temporary allocator: linear probe for a free record number.
     * TODO: Read and update $MFT bitmap properly and journal with LFS.
     */
    NTSTATUS Status;
    ULONG reservedStart = 24; // reserved MFT entries (metadata files)

    // Query $MFT::$BITMAP attribute
    PFileRecord BitmapOwner;
    PAttribute  BitmapAttr;
    Status = GetFileAttributeFromFileRecordNumber(TypeBitmap, NULL, _MFT, &BitmapOwner, &BitmapAttr);
    if (!NT_SUCCESS(Status) || !BitmapAttr)
    {
        // Fallback: linear probe without bitmap
        BOOLEAN inUse;
        ULONG probe = reservedStart;
        for (;; ++probe)
        {
            Status = IsFileRecordNumberInUse(probe, &inUse);
            if (!NT_SUCCESS(Status)) return Status;
            if (!inUse) { *FileRecordNumber = probe; return STATUS_SUCCESS; }
            if (probe > 0x00FFFFFF) return STATUS_DISK_FULL;
        }
    }

    // Read bitmap and scan for a zero bit
    ULONGLONG totalBytes = GetAttributeDataSize(BitmapAttr);
    ULONGLONG offset = 0;
    ULONG chunk = 4096;
    PUCHAR map = (PUCHAR)ExAllocatePoolWithTag(PagedPool, chunk, TAG_MFT);
    if (!map) { delete BitmapOwner; return STATUS_INSUFFICIENT_RESOURCES; }
    ULONG foundBit = 0xFFFFFFFF;
    while (offset < totalBytes)
    {
        ULONGLONG remainBytes = totalBytes - offset;
        ULONG toRead = (remainBytes < (ULONGLONG)chunk) ? (ULONG)remainBytes : chunk;
        RtlZeroMemory(map, toRead);
        ULONG remain = toRead;
        Status = MFTFile->CopyData(TypeBitmap, NULL, map, &remain, offset);
        if (!NT_SUCCESS(Status)) { ExFreePoolWithTag(map, TAG_MFT); delete BitmapOwner; return Status; }
        ULONG got = toRead - remain;
        if (got == 0) break;

        for (ULONG i = 0; i < got; ++i)
        {
            UCHAR b = map[i];
            if (b != 0xFF)
            {
                // find first zero bit in this byte
                for (ULONG bit = 0; bit < 8; ++bit)
                {
                    if (!(b & (1u << bit)))
                    {
                        ULONG globalBit = (ULONG)((offset + i) * 8 + bit);
                        if (globalBit >= reservedStart)
                        {
                            foundBit = globalBit;
                            goto found;
                        }
                    }
                }
            }
        }

        offset += got;
    }

found:
    if (foundBit == 0xFFFFFFFF)
    {
        ExFreePoolWithTag(map, TAG_MFT);
        delete BitmapOwner;
        return STATUS_DISK_FULL;
    }

    // Mark the found bit and write back only the affected byte at its offset
    ULONGLONG byteOff = (ULONGLONG)(foundBit >> 3);
    UCHAR newByte;
    ULONG one = 1;
    // Read current byte to modify
    Status = MFTFile->CopyData(TypeBitmap, NULL, &newByte, &one, byteOff);
    if (!NT_SUCCESS(Status)) { ExFreePoolWithTag(map, TAG_MFT); delete BitmapOwner; return Status; }
    newByte = (UCHAR)(newByte | (1u << (foundBit & 7)));
    {
        LARGE_INTEGER writeOff; writeOff.QuadPart = byteOff;
        ULONG oneLen = 1;
        Status = MFTFile->WriteFileData(TypeBitmap, NULL, &newByte, &oneLen, &writeOff);
    }

    // Update the in-memory MFT file's StandardInformation/DataSize if needed
    // (not strictly necessary, but keeps sizes consistent for cache)
    // No-op for now

    ExFreePoolWithTag(map, TAG_MFT);
    delete BitmapOwner;
    if (!NT_SUCCESS(Status)) return Status;
    *FileRecordNumber = foundBit;
    return STATUS_SUCCESS;
}