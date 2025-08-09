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
    PAttribute IndexRootAttr = DirFile->GetAttribute(TypeIndexRoot, NULL);
    if (!IndexRootAttr || IndexRootAttr->IsNonResident)
        return STATUS_FILE_CORRUPT_ERROR;

    // Build an IndexEntry for the new file name pointing to the file reference
    USHORT nameBytes = FileToAdd->NameLength * sizeof(WCHAR);
    USHORT streamBytes = (USHORT)(sizeof(FileNameEx) - sizeof(WCHAR) + nameBytes);
    USHORT entryLen = (USHORT)ROUND_UP(FIELD_OFFSET(IndexEntry, IndexStream) + streamBytes, 8);

    // Find insertion point: for now, append before the end entry in the root index
    PIndexRootEx root = (PIndexRootEx)GetResidentDataPointer(IndexRootAttr);
    PUCHAR rootData = (PUCHAR)&root->Header;
    ULONG totalSize = root->Header.TotalIndexSize;
    ULONG indexOffset = root->Header.IndexOffset;
    PIndexEntry entry = (PIndexEntry)(rootData + indexOffset);
    PUCHAR end = (PUCHAR)rootData + totalSize;

    // Walk to last entry
    while ((PUCHAR)entry < end && !(entry->Flags & INDEX_ENTRY_END))
    {
        entry = (PIndexEntry)((PUCHAR)entry + entry->EntryLength);
    }

    if ((PUCHAR)entry >= end)
        return STATUS_FILE_CORRUPT_ERROR;

    // Ensure room in resident index root; if insufficient, bail for now
    ULONG needed = entryLen;
    if ((totalSize + needed) > IndexRootAttr->Resident.DataLength)
    {
        DPRINT1("Index root expansion not implemented yet (promote to IndexAllocation).\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    // Shift end marker forward to make space
    SIZE_T tail = (SIZE_T)(end - (PUCHAR)entry);
    RtlMoveMemory((PUCHAR)entry + needed, entry, tail);
    root->Header.TotalIndexSize += (UINT16)needed;

    // Fill the new entry
    entry->Data.Directory.IndexedFile = FileReferenceNumber;
    entry->StreamLength = streamBytes;
    entry->EntryLength = entryLen;
    entry->Flags = 0; // leaf entry

    PFileNameEx fn = (PFileNameEx)entry->IndexStream;
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