/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

NTSTATUS
Directory::AddFileToDirectory(_In_ PFileNameEx FileToAdd,
                              _In_ ULONGLONG FileReferenceNumber)
{
    /* Algorithm:
     *    - Log to LFS
     *    - Manipulate the btree as needed to add the entry
     *    - Write to disk
     */

    DPRINT1("AddFileToDirectory() called!\n");

    NTSTATUS Status;
    if (!DirFile) return STATUS_INVALID_PARAMETER;
    PAttribute IndexRootAttr = DirFile->GetAttribute(TypeIndexRoot, NULL);
    if (!IndexRootAttr || IndexRootAttr->IsNonResident)
        return STATUS_FILE_CORRUPT_ERROR;

    // Build an IndexEntry for the new file name pointing to the file reference
    USHORT nameBytes = FileToAdd->NameLength * sizeof(WCHAR);
    USHORT streamBytes = (USHORT)(sizeof(FileNameEx) - sizeof(WCHAR) + nameBytes);
    USHORT entryLen = (USHORT)ROUND_UP(FIELD_OFFSET(IndexEntry, IndexStream) + streamBytes, 8);

    // Access root index data
    PIndexRootEx root = (PIndexRootEx)GetResidentDataPointer(IndexRootAttr);
    PUCHAR rootData = (PUCHAR)&root->Header;
    ULONG totalSize = root->Header.TotalIndexSize;
    ULONG indexOffset = root->Header.IndexOffset;
    PIndexEntry entry = (PIndexEntry)(rootData + indexOffset);
    PUCHAR end = (PUCHAR)rootData + totalSize;
    // Sanity: TotalIndexSize must not exceed resident data length
    if ((FIELD_OFFSET(IndexRootEx, Header) + totalSize) > IndexRootAttr->Resident.DataLength)
        return STATUS_FILE_CORRUPT_ERROR;

    // Walk to last entry (validate bounds)
    while ((PUCHAR)entry < end)
    {
        if (entry->EntryLength == 0) return STATUS_FILE_CORRUPT_ERROR;
        if (entry->Flags & INDEX_ENTRY_END) break;
        entry = (PIndexEntry)((PUCHAR)entry + entry->EntryLength);
    }

    if ((PUCHAR)entry >= end)
        return STATUS_FILE_CORRUPT_ERROR;

    ULONG needed = entryLen;
    ULONG newTotal = totalSize + needed;

    // Ensure we only do in-place updates when the expanded index fits entirely
    // within the current resident data length (including the IndexRootEx preamble)
    if ((FIELD_OFFSET(IndexRootEx, Header) + newTotal) <= IndexRootAttr->Resident.DataLength)
    {
        // In-place insert within current resident data length
        SIZE_T tail = (SIZE_T)(end - (PUCHAR)entry);
        RtlMoveMemory((PUCHAR)entry + needed, entry, tail);
        root->Header.TotalIndexSize = (UINT16)newTotal;
        root->Header.AllocatedSize = (UINT16)ROUND_UP(newTotal, 8);

        PIndexEntry newEntry = entry;
        newEntry->Data.Directory.IndexedFile = FileReferenceNumber;
        newEntry->StreamLength = streamBytes;
        newEntry->EntryLength = entryLen;
        newEntry->Flags = 0; // leaf entry

        PFileNameEx fn = (PFileNameEx)newEntry->IndexStream;
        fn->ParentFileReference = 0; // not used in index stream
        fn->CreationTime = 0;
        fn->LastWriteTime = 0;
        fn->ChangeTime = 0;
        fn->LastAccessTime = 0;
        fn->AllocatedSize = 0;
        fn->DataSize = 0;
        fn->Flags = 0;
        fn->Extended.EAInfo.PackedEASize = 0;
        fn->Extended.EAInfo.Padding = 0;
        fn->NameLength = FileToAdd->NameLength;
        fn->NameType = NAME_TYPE_WIN32;
        RtlCopyMemory(fn->Name, FileToAdd->Name, nameBytes);
    }
    else
    {
        // Need to grow the resident index root data. Build a new buffer and write it using UpdateResidentData.
        // Allocate buffer for entire IndexRootEx (preamble + header.TotalIndexSize)
        ULONG copyLen = FIELD_OFFSET(IndexRootEx, Header) + totalSize;
        PUCHAR newBuf = (PUCHAR)ExAllocatePoolWithTag(PagedPool, copyLen + needed, TAG_BTREE);
        if (!newBuf) return STATUS_INSUFFICIENT_RESOURCES;
        // Copy existing IndexRootEx preamble + header+entries into new buffer
        RtlCopyMemory(newBuf, root, copyLen);

        // Pointers within new buffer
        PIndexRootEx newRoot = (PIndexRootEx)newBuf;
        PUCHAR newRootData = (PUCHAR)newRoot;

        // Locate end marker entry in the new buffer by walking from IndexOffset
        PIndexEntry scan = (PIndexEntry)(newRootData + newRoot->Header.IndexOffset);
        PUCHAR newEnd = newRootData + newRoot->Header.TotalIndexSize;
        while ((PUCHAR)scan < newEnd)
        {
            if (scan->EntryLength == 0) { ExFreePoolWithTag(newBuf, TAG_BTREE); return STATUS_FILE_CORRUPT_ERROR; }
            if (scan->Flags & INDEX_ENTRY_END) break;
            scan = (PIndexEntry)((PUCHAR)scan + scan->EntryLength);
        }
        if ((PUCHAR)scan >= newEnd)
        {
            ExFreePoolWithTag(newBuf, TAG_BTREE);
            return STATUS_FILE_CORRUPT_ERROR;
        }

        // Insert new entry before end marker: shift current end marker and trailing bytes
        SIZE_T endTail = (SIZE_T)(newEnd - (PUCHAR)scan);
        RtlMoveMemory((PUCHAR)scan + needed, scan, endTail);
        newRoot->Header.TotalIndexSize = (UINT16)newTotal;
        newRoot->Header.AllocatedSize = (UINT16)ROUND_UP(newTotal, 8);

        PIndexEntry newEntry = scan;
        newEntry->Data.Directory.IndexedFile = FileReferenceNumber;
        newEntry->StreamLength = streamBytes;
        newEntry->EntryLength = entryLen;
        newEntry->Flags = 0;
        PFileNameEx fn = (PFileNameEx)newEntry->IndexStream;
        fn->ParentFileReference = 0;
        fn->CreationTime = 0;
        fn->LastWriteTime = 0;
        fn->ChangeTime = 0;
        fn->LastAccessTime = 0;
        fn->AllocatedSize = 0;
        fn->DataSize = 0;
        fn->Flags = 0;
        fn->Extended.EAInfo.PackedEASize = 0;
        fn->Extended.EAInfo.Padding = 0;
        fn->NameLength = FileToAdd->NameLength;
        fn->NameType = NAME_TYPE_WIN32;
        RtlCopyMemory(fn->Name, FileToAdd->Name, nameBytes);

        // Write back with attribute growth
        ULONG newLenVar = FIELD_OFFSET(IndexRootEx, Header) + newTotal;
        Status = DirFile->UpdateResidentData(IndexRootAttr, newBuf, &newLenVar, 0);
        ExFreePoolWithTag(newBuf, TAG_BTREE);
        if (!NT_SUCCESS(Status)) return Status;
    }

    // Update the file record on disk (write back directory's file record)
    Status = Volume->MFT->WriteFileRecordToMFT(DirFile);
    return Status;
}

NTSTATUS
Directory::RemoveFileFromDirectory(_In_ PBTreeKey FileToRemove)
{
    /* Algorithm:
     *    - Log to LFS
     *    - Manipulate the btree as needed to remove the entry
     *    - Write to disk
     */

    DPRINT1("RemoveFileFromDirectory() called!\n");
    DPRINT1("Logging using LFS is not implemented!\n");
    return STATUS_NOT_IMPLEMENTED;
}