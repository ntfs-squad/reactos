/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"
#include "ntfs_tags.h"

#define BytesPerIndexRecord(DiskVolume) \
(BytesPerCluster(DiskVolume) * DiskVolume->ClustersPerIndexRecord)

// Used for LoadDirectory()
#define MayHaveShortKey(SearchKey) \
!(SearchKey->Flags & DIR_KEY_8DOT3) \
&& !(SearchKey->Entry->Flags & INDEX_ENTRY_END)

NTSTATUS
Directory::VerifyUpdateSequenceArray(PNTFSRecordHeader Record)
{
    USHORT *USA;
    USHORT USANumber;
    USHORT USACount;
    USHORT *Block;
    ULONG  BytesPerSector;

    BytesPerSector = DiskVolume->BytesPerSector;
    USA = (USHORT*)((PCHAR)Record + Record->UpdateSequenceOffset);
    USANumber = *(USA++);
    USACount = Record->SizeOfUpdateSequence - 1; // Exclude the USA Number.
    Block = (USHORT*)((PCHAR)Record + BytesPerSector - 2);

    while (USACount)
    {
        if (*Block != USANumber)
        {
            DPRINT1("Mismatch with USA: %u read, %u expected\n" , *Block, USANumber);
            return STATUS_UNSUCCESSFUL;
        }
        *Block = *(USA++);
        Block = (USHORT*)((PCHAR)Block + BytesPerSector);
        USACount--;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
Directory::CreateNode(_In_    PFileRecord File,
                      _In_    PAttribute  IndexAllocationAttribute,
                      _Inout_ PBTreeKey   ParentNodeKey)
{
    NTSTATUS Status;
    PBTreeNode NewNode;
    PBTreeKey CurrentKey, NextKey;
    PIndexBuffer NodeBuffer;
    PIndexEntry CurrentEntry;
    PULONGLONG VCN;
    ULONG IndexBufferSize;
    ULONG_PTR EndOfIndexBuffer;

    // Get VCN from the end of the node entry
    VCN = (PULONGLONG)((char*)ParentNodeKey->Entry + ParentNodeKey->Entry->EntryLength - sizeof(ULONGLONG));
    IndexBufferSize = BytesPerIndexRecord(DiskVolume);

    // Create the new node and first key.
    NewNode = new(PagedPool, TAG_BTREE) BTreeNode();
    CurrentKey = new(PagedPool, TAG_BTREE) BTreeKey();
    NewNode->FirstKey = CurrentKey;
    NodeBuffer = (PIndexBuffer)ExAllocatePoolWithTag(PagedPool,
                                                     IndexBufferSize,
                                                     TAG_NTFS);

    // TODO: Confirm index bitmap has this node marked as in-use
    Status = File->CopyData(IndexAllocationAttribute,
                            (PUCHAR)NodeBuffer,
                            &IndexBufferSize,
                            GetAllocationOffsetFromVCN(*VCN));

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to create node! Unable to copy data!\n");
        __debugbreak();
        return STATUS_NOT_FOUND;
    }

    // Shamelessly ripped from old driver.
    Status = VerifyUpdateSequenceArray(&NodeBuffer->RecordHeader);

    ASSERT(NT_SUCCESS(Status));
    ASSERT(RtlCompareMemory(NodeBuffer->RecordHeader.TypeID, "INDX", 4) == 4);
    ASSERT(NodeBuffer->VCN == *VCN);
    ASSERT(IndexBufferSize == 0);

    // Walk through the index and create keys for all the entries
    CurrentEntry = (PIndexEntry)((ULONG_PTR)(&NodeBuffer->IndexHeader)
                                 + NodeBuffer->IndexHeader.IndexOffset);
    EndOfIndexBuffer = (ULONG_PTR)(&NodeBuffer->IndexHeader) + NodeBuffer->IndexHeader.TotalIndexSize;

    while ((ULONG_PTR)CurrentEntry < EndOfIndexBuffer)
    {
        // Allocate memory for the current entry
        CurrentKey->Entry = (PIndexEntry)ExAllocatePoolWithTag(PagedPool,
                                                               CurrentEntry->EntryLength,
                                                               TAG_NTFS);
        // Add the parent node
        CurrentKey->ParentNodeKey = ParentNodeKey;

        // Copy entry into key
        RtlCopyMemory(CurrentKey->Entry,
                      CurrentEntry,
                      CurrentEntry->EntryLength);

        if (CurrentKey->Entry->Flags & INDEX_ENTRY_NODE)
        {
            Status = CreateNode(File,
                                IndexAllocationAttribute,
                                CurrentKey);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to create subnode!\n");
                __debugbreak();
                // return STATUS_NOT_FOUND;
            }
        }

        if (!(CurrentEntry->Flags & INDEX_ENTRY_END))
        {
            // Create the next key
            NextKey = new(PagedPool, TAG_BTREE) BTreeKey();
            CurrentKey->NextKey = NextKey;

            // Advance to next entry
            CurrentEntry = (PIndexEntry)((ULONG_PTR)CurrentEntry
                                         + CurrentEntry->EntryLength);
            CurrentKey = NextKey;
        }
        else
        {
            CurrentKey->NextKey = NULL;
            break;
        }
    }

    delete NodeBuffer;
    NewNode->VCN = *VCN;
    ParentNodeKey->ChildNode = NewNode;

    return STATUS_SUCCESS;
}

NTSTATUS
Directory::CreateRootNode(_In_  PFileRecord File,
                          _Out_ PBTreeNode *NewRootNode)
{
    NTSTATUS Status;
    PAttribute IndexRootAttribute, IndexAllocationAttribute;
    PIndexRootEx IndexRootData;
    PBTreeNode RootNode;
    PBTreeKey CurrentKey, NextKey;
    PIndexEntry CurrentEntry;
    ULONG_PTR EndOfIndexRootData;

    // Get $INDEX_ROOT attribute.
    IndexRootAttribute = File->GetAttribute(TypeIndexRoot, NULL);

    // If it's a directory, it should have an index root.
    ASSERT(IndexRootAttribute);

    /* Set up pointers.
     * Note: IndexAllocationAttribute may be null. That is okay!
     */
    IndexRootData = (PIndexRootEx)GetResidentDataPointer(IndexRootAttribute);
    IndexAllocationAttribute = File->GetAttribute(TypeIndexAllocation, L"$I30");
    EndOfIndexRootData = ((ULONG_PTR)IndexRootData) +
                         (FIELD_OFFSET(IndexRootEx, Header)) +
                         (IndexRootData->Header.TotalIndexSize);

    // Make sure we won't try reading past the attribute-end
    if ((FIELD_OFFSET(IndexRootEx, Header) + IndexRootData->Header.TotalIndexSize) > IndexRootAttribute->Resident.DataLength)
    {
        DPRINT1("Filesystem corruption detected!\n");
        __debugbreak();
        return STATUS_FILE_CORRUPT_ERROR;
    }

    // Initialize variables
    RootNode = new(PagedPool, TAG_BTREE) BTreeNode();
    CurrentKey = new(PagedPool, TAG_BTREE) BTreeKey();
    RootNode->FirstKey = CurrentKey;

    CurrentEntry = (PIndexEntry)(((ULONG_PTR)IndexRootData) +
                                 (FIELD_OFFSET(IndexRootEx, Header)) +
                                 (IndexRootData->Header.IndexOffset));

    while ((ULONG_PTR)CurrentEntry < EndOfIndexRootData)
    {
        ASSERT(CurrentEntry->EntryLength);

        // Create current entry
        CurrentKey->Entry = (PIndexEntry)ExAllocatePoolWithTag(NonPagedPool,
                                                               CurrentEntry->EntryLength,
                                                               TAG_NTFS);

        // Copy the current entry to its key
        RtlCopyMemory(CurrentKey->Entry,
                      CurrentEntry,
                      CurrentEntry->EntryLength);

        if (CurrentEntry->Flags & INDEX_ENTRY_NODE)
        {
            ASSERT(IndexAllocationAttribute);

            // Create child node
            Status = CreateNode(File,
                                IndexAllocationAttribute,
                                CurrentKey);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to create node!\n");
                __debugbreak();
                goto Failed;
            }
        }

        if (!(CurrentEntry->Flags & INDEX_ENTRY_END))
        {
            // Create next key
            NextKey = new(PagedPool, TAG_BTREE) BTreeKey();
            CurrentKey->NextKey = NextKey;

            // Advance to the next entry
            CurrentEntry = (PIndexEntry)((ULONG_PTR)CurrentEntry +
                                         CurrentEntry->EntryLength);
            CurrentKey = NextKey;
        }

        else
        {
            // We've copied the last entry.
            break;
        }
    }

    *NewRootNode = RootNode;
    return STATUS_SUCCESS;
Failed:
    delete RootNode;
    return Status;
}

NTSTATUS
Directory::LoadDirectory(_In_ PFileRecord File)
{
    NTSTATUS Status;
    PBTreeKey SearchKey, ShortNameKey;
    // PAttribute BitmapAttribute;

    // This only works on files that are directories.
    ASSERT(File->Header->Flags & FR_IS_DIRECTORY);

    /* First, we need to get the index allocation bitmap attribute
     * to determine what index entries are marked as in use.
     */
    // TODO: Implement.
    // BitmapAttribute = File->GetAttribute(TypeBitmap, L"$I30");
    // if (!BitmapAttribute)
    // {
    //     DPRINT1("Failed to find $BITMAP attribute!\n");
    //     return STATUS_NOT_FOUND;
    // }

    // Get the root node for the directory
    Status = CreateRootNode(File, &RootNode);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to create root node!\n");
        return STATUS_NOT_FOUND;
    }

    CurrentKey = RootNode->FirstKey;
    SearchKey = CurrentKey;

    // Mark short name keys accordingly.
    // TODO: If short name generation is disabled for this volume, we can skip this.
    while(SearchKey)
    {
        if (MayHaveShortKey(SearchKey))
        {
            ShortNameKey = GetShortNameKey(SearchKey, FALSE);
            if (ShortNameKey)
                ShortNameKey->Flags |= DIR_KEY_8DOT3;
        }

        SearchKey = GetNextKey(SearchKey);
    }

    return Status;
}

BOOLEAN
Directory::DoesFileNameMatch(PUNICODE_STRING NameFilter,
                             PBTreeKey Key,
                             BOOLEAN IgnoreCase)
{
    UNICODE_STRING FileNameString;
    PFileNameEx FileNameData;

    if (Key->Entry->Flags & INDEX_ENTRY_END)
    {
        // This is a dummy key, it will not match with any file.
        return FALSE;
    }

    FileNameData = GetFileName(Key);
    RtlInitEmptyUnicodeString(&FileNameString,
                              FileNameData->Name,
                              ((FileNameData->NameLength) * sizeof(WCHAR)));
    FileNameString.Length = FileNameString.MaximumLength;

    if (IgnoreCase)
    {
        /* Note: if we want to do case-insensitive searching, we must make
         * NameFilter all uppercase.
         *
         * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtlisnameinexpression
        */

        // TODO: Should this be replaced with DiskVolume->UpcaseWideString()?
        NTSTATUS Status = RtlUpcaseUnicodeString(NameFilter, NameFilter, FALSE);
        if (!NT_SUCCESS(Status))
        {
            // Failed to upcase name filter! Perform case sensitive matching...
            __debugbreak();
            IgnoreCase = FALSE;
        }
    }

    return FsRtlIsNameInExpression(NameFilter,
                                   &FileNameString,
                                   IgnoreCase,
                                   NULL);
}

PBTreeKey
Directory::GetShortNameKey(_In_ PBTreeKey Key,
                           _In_ BOOLEAN SkipNonShortNames)
{
    /* Search file tree for the next entry with the same file record
     * number (FRN).
     *
     * NOTE: I have seen short file name entries 3+ keys after
     * the long file name entry, but never before the long name entry
     * (see: system32). Most of the time, it's the next key.
     *
     * If you do observe a short file name entry before the long file
     * name entry, please update the search algorithm.
     */

    PBTreeKey FoundKey;
    PFileNameEx FileNameData;
    ULONGLONG TargetFRN;

    // If this is a dummy key, there is no short name
    if (Key->Entry->Flags & INDEX_ENTRY_END)
    {
#ifdef NTFS_DEBUG
        DPRINT1("Tried to find short name for a dummy key!\n");
        DPRINT1("FIXME: Rework whatever algorithm to prevent this.\n");
#endif
        return NULL;
    }

    // If the key is already a legal short name, there isn't another short name
    FileNameData = GetFileName(Key);
    if (IsLegal8Dot3ShortName(FileNameData->Name, FileNameData->NameLength))
        return NULL;

    TargetFRN = GetFRNFromFileRef(FileRef(Key));
    FoundKey = GetNextKey(Key);

    /* This algorithm does not go back to search from the beginning of the tree.
     * If you observe a short name bug, this is probably it.
     */
    while (FoundKey)
    {
        if (!SkipNonShortNames ||
            FoundKey->Flags & DIR_KEY_8DOT3)
        {
            FileNameData = GetFileName(FoundKey);
            if (GetFRNFromFileRef(FileRef(FoundKey)) == TargetFRN
                && IsLegal8Dot3ShortName(FileNameData->Name, FileNameData->NameLength))
            {
                return FoundKey;
            }
        }
        FoundKey = GetNextKey(FoundKey);
    }

    /* If we don't find the short name, no short file name was generated.
     * This is not necessarily an error.
     */

    return NULL;
}

Directory::Directory(_In_ PVolume DiskVolume)
{
    this->DiskVolume = DiskVolume;
}

Directory::Directory(_In_ PVolume DiskVolume,
                     _In_ PFileRecord File)
                    : Directory(DiskVolume)
{
    LoadDirectory(File);
}