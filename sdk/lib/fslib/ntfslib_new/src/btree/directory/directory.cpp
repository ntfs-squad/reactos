/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

typedef struct _SHORT_NAME_PAIR
{
    ULONGLONG FileReference;
    PBTreeKey LongNameKey;
    PBTreeKey ShortNameKey;
    BOOLEAN Occupied;
} SHORT_NAME_PAIR, *PSHORT_NAME_PAIR;

static ULONG
HashFileReference(_In_ ULONGLONG FileReference)
{
    return (ULONG)(FileReference ^ (FileReference >> 32)) * 2654435761UL;
}

static BOOLEAN
IsIndexEntryValid(_In_ PIndexEntry Entry,
                  _In_ ULONG Remaining)
{
    ULONG HeaderSize = FIELD_OFFSET(IndexEntry, IndexStream);

    return Remaining >= HeaderSize &&
           Entry->EntryLength >= HeaderSize &&
           Entry->EntryLength <= Remaining &&
           Entry->StreamLength <= Entry->EntryLength - HeaderSize &&
           (!(Entry->Flags & INDEX_ENTRY_NODE) ||
            Entry->EntryLength >= HeaderSize + sizeof(ULONGLONG));
}

NTSTATUS
Directory::CreateNode(_In_    PFileRecord File,
                      _In_    PAttribute  IndexAllocationAttribute,
                      _In_    PDataRun    IndexAllocationRuns,
                      _In_    PAttribute  BitmapAttribute,
                      _Inout_ PBTreeKey   ParentNodeKey)
{
    NTSTATUS Status;
    PBTreeNode NewNode;
    PBTreeKey CurrentKey, NextKey;
    PIndexBuffer NodeBuffer;
    PIndexEntry CurrentEntry;
    PULONGLONG VCN;
    ULONG IndexBufferSize;
    ULONG BitmapByte, BitmapLength;
    ULONG IndexRecordSize;
    ULONG_PTR EndOfIndexBuffer;
    UCHAR BitmapValue, BitmapMask;
    BOOLEAN FoundEndEntry;

    // Get VCN from the end of the node entry
    VCN = GetSubnodeVCN(ParentNodeKey->Entry);
    IndexBufferSize = BytesPerIndexRecord(DiskVolume);
    IndexRecordSize = IndexBufferSize;

    /* A child reference is valid only when its index buffer is allocated in
     * the named index's $BITMAP.
     */
    if (!BitmapAttribute)
        return STATUS_FILE_CORRUPT_ERROR;

    BitmapByte = (ULONG)(GetAllocationOffsetFromVCN(*VCN) /
                         IndexBufferSize) >> 3;
    BitmapMask = 1 << ((GetAllocationOffsetFromVCN(*VCN) /
                        IndexBufferSize) & 7);
    BitmapLength = sizeof(BitmapValue);
    Status = File->CopyData(BitmapAttribute,
                            &BitmapValue,
                            &BitmapLength,
                            BitmapByte);
    if (!NT_SUCCESS(Status) || BitmapLength != 0)
        return NT_SUCCESS(Status) ? STATUS_END_OF_FILE : Status;

    if (!(BitmapValue & BitmapMask))
        return STATUS_FILE_CORRUPT_ERROR;

    // Create the new node and first key.
    NewNode = new(PagedPool, TAG_BTREE) BTreeNode();
    if (!NewNode)
        return STATUS_INSUFFICIENT_RESOURCES;

    CurrentKey = new(PagedPool, TAG_BTREE) BTreeKey();
    if (!CurrentKey)
    {
        delete NewNode;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NewNode->FirstKey = CurrentKey;
    ParentNodeKey->ChildNode = NewNode;
    NodeBuffer = (PIndexBuffer)NtfsAllocatePoolWithTag(PagedPool,
                                                       IndexBufferSize,
                                                       TAG_NTFS);
    if (!NodeBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = File->CopyData(IndexAllocationAttribute,
                            IndexAllocationRuns,
                            (PUCHAR)NodeBuffer,
                            &IndexBufferSize,
                            GetAllocationOffsetFromVCN(*VCN));

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to create node! Unable to copy data!\n");
        NtfsFreePool(NodeBuffer);
        return Status;
    }

    Status = NtfsApplyFixup(&NodeBuffer->RecordHeader,
                            IndexRecordSize,
                            DiskVolume->BytesPerSector);
    if (!NT_SUCCESS(Status) ||
        RtlCompareMemory(NodeBuffer->RecordHeader.TypeID, "INDX", 4) != 4 ||
        NodeBuffer->VCN != *VCN ||
        IndexBufferSize != 0 ||
        NodeBuffer->IndexHeader.IndexOffset >
            NodeBuffer->IndexHeader.TotalIndexSize ||
        (ULONG)FIELD_OFFSET(IndexBuffer, IndexHeader) +
            NodeBuffer->IndexHeader.TotalIndexSize > IndexRecordSize)
    {
        NtfsFreePool(NodeBuffer);
        return STATUS_FILE_CORRUPT_ERROR;
    }

    // Walk through the index and create keys for all the entries
    CurrentEntry = (PIndexEntry)((ULONG_PTR)(&NodeBuffer->IndexHeader)
                                 + NodeBuffer->IndexHeader.IndexOffset);
    EndOfIndexBuffer = (ULONG_PTR)(&NodeBuffer->IndexHeader) + NodeBuffer->IndexHeader.TotalIndexSize;
    FoundEndEntry = FALSE;

    while ((ULONG_PTR)CurrentEntry < EndOfIndexBuffer)
    {
        if (!IsIndexEntryValid(
                CurrentEntry,
                (ULONG)(EndOfIndexBuffer - (ULONG_PTR)CurrentEntry)))
        {
            NtfsFreePool(NodeBuffer);
            return STATUS_FILE_CORRUPT_ERROR;
        }

        // Allocate memory for the current entry
        CurrentKey->Entry = (PIndexEntry)NtfsAllocatePoolWithTag(PagedPool,
                                                                 CurrentEntry->EntryLength,
                                                                 TAG_NTFS);
        if (!CurrentKey->Entry)
        {
            NtfsFreePool(NodeBuffer);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
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
                                IndexAllocationRuns,
                                BitmapAttribute,
                                CurrentKey);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to create subnode!\n");
                NtfsFreePool(NodeBuffer);
                return Status;
            }
        }

        if (!(CurrentEntry->Flags & INDEX_ENTRY_END))
        {
            // Create the next key
            NextKey = new(PagedPool, TAG_BTREE) BTreeKey();
            if (!NextKey)
            {
                NtfsFreePool(NodeBuffer);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            CurrentKey->NextKey = NextKey;

            // Advance to next entry
            CurrentEntry = (PIndexEntry)((ULONG_PTR)CurrentEntry
                                         + CurrentEntry->EntryLength);
            CurrentKey = NextKey;
        }
        else
        {
            CurrentKey->NextKey = NULL;
            FoundEndEntry = TRUE;
            break;
        }
    }

    NtfsFreePool(NodeBuffer);
    if (!FoundEndEntry)
        return STATUS_FILE_CORRUPT_ERROR;

    NewNode->VCN = *VCN;
    return STATUS_SUCCESS;
}

NTSTATUS
Directory::CreateRootNode(_In_  PFileRecord File,
                          _Out_ PBTreeNode *NewRootNode)
{
    NTSTATUS Status;
    PAttribute IndexRootAttribute, IndexAllocationAttribute, BitmapAttribute;
    PIndexRootEx IndexRootData;
    PBTreeNode RootNode;
    PBTreeKey CurrentKey, NextKey;
    PIndexEntry CurrentEntry;
    PDataRun IndexAllocationRuns = NULL;
    ULONG_PTR EndOfIndexRootData;
    BOOLEAN FoundEndEntry;

    *NewRootNode = NULL;

    // Get $INDEX_ROOT attribute.
    IndexRootAttribute = File->GetAttribute(TypeIndexRoot, NULL);

    // If it's a directory, it must have an index root.
    if (!IndexRootAttribute || IndexRootAttribute->IsNonResident)
        return STATUS_FILE_CORRUPT_ERROR;

    if (IndexRootAttribute->Resident.DataLength <
        FIELD_OFFSET(IndexRootEx, Header) + sizeof(IndexNodeHeader))
    {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    /* Set up pointers.
     * Note: IndexAllocationAttribute may be null. That is okay!
     */
    IndexRootData = (PIndexRootEx)GetResidentDataPointer(IndexRootAttribute);
    IndexAllocationAttribute = File->GetAttribute(
        TypeIndexAllocation,
        const_cast<PWSTR>(L"$I30"));
    BitmapAttribute = File->GetAttribute(
        TypeBitmap,
        const_cast<PWSTR>(L"$I30"));
    EndOfIndexRootData = ((ULONG_PTR)IndexRootData) +
                         (FIELD_OFFSET(IndexRootEx, Header)) +
                         (IndexRootData->Header.TotalIndexSize);

    // Make sure we won't try reading past the attribute-end
    if (((ULONG)FIELD_OFFSET(IndexRootEx, Header) +
         IndexRootData->Header.TotalIndexSize) >
        IndexRootAttribute->Resident.DataLength ||
        IndexRootData->Header.IndexOffset >
            IndexRootData->Header.TotalIndexSize)
    {
        DPRINT1("Filesystem corruption detected!\n");
        __debugbreak();
        return STATUS_FILE_CORRUPT_ERROR;
    }

    /* Decode the index allocation runs once so every node created below
     * doesn't have to re-decode them.
     */
    if (IndexAllocationAttribute && IndexAllocationAttribute->IsNonResident)
        IndexAllocationRuns = File->FindNonResidentData(IndexAllocationAttribute);

    // Initialize variables
    RootNode = new(PagedPool, TAG_BTREE) BTreeNode();
    if (!RootNode)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Failed;
    }

    CurrentKey = new(PagedPool, TAG_BTREE) BTreeKey();
    if (!CurrentKey)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Failed;
    }

    RootNode->FirstKey = CurrentKey;

    CurrentEntry = (PIndexEntry)(((ULONG_PTR)IndexRootData) +
                                 (FIELD_OFFSET(IndexRootEx, Header)) +
                                 (IndexRootData->Header.IndexOffset));
    FoundEndEntry = FALSE;

    while ((ULONG_PTR)CurrentEntry < EndOfIndexRootData)
    {
        if (!IsIndexEntryValid(
                CurrentEntry,
                (ULONG)(EndOfIndexRootData - (ULONG_PTR)CurrentEntry)))
        {
            Status = STATUS_FILE_CORRUPT_ERROR;
            goto Failed;
        }

        // Create current entry
        CurrentKey->Entry = (PIndexEntry)NtfsAllocatePoolWithTag(NonPagedPool,
                                                                 CurrentEntry->EntryLength,
                                                                 TAG_NTFS);
        if (!CurrentKey->Entry)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Failed;
        }

        // Copy the current entry to its key
        RtlCopyMemory(CurrentKey->Entry,
                      CurrentEntry,
                      CurrentEntry->EntryLength);

        if (CurrentEntry->Flags & INDEX_ENTRY_NODE)
        {
            if (!IndexAllocationAttribute ||
                !IndexAllocationAttribute->IsNonResident ||
                !IndexAllocationRuns)
            {
                Status = STATUS_FILE_CORRUPT_ERROR;
                goto Failed;
            }

            // Create child node
            Status = CreateNode(File,
                                IndexAllocationAttribute,
                                IndexAllocationRuns,
                                BitmapAttribute,
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
            if (!NextKey)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Failed;
            }
            CurrentKey->NextKey = NextKey;

            // Advance to the next entry
            CurrentEntry = (PIndexEntry)((ULONG_PTR)CurrentEntry +
                                         CurrentEntry->EntryLength);
            CurrentKey = NextKey;
        }

        else
        {
            // We've copied the last entry.
            FoundEndEntry = TRUE;
            break;
        }
    }

    if (!FoundEndEntry)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Failed;
    }

    FreeDataRun(IndexAllocationRuns);
    *NewRootNode = RootNode;
    return STATUS_SUCCESS;
Failed:
    FreeDataRun(IndexAllocationRuns);
    NtfsDestroyBTreeNode(RootNode);
    return Status;
}

NTSTATUS
Directory::LoadDirectory(_In_ PFileRecord File)
{
    NTSTATUS Status;
    PBTreeKey SearchKey;
    PSHORT_NAME_PAIR PairTable;
    ULONG EntryCount, PairIndex, PairTableSize;
    ULONGLONG FileReference;
    // This only works on files that are directories.
    ASSERT(File->Header->Flags & FR_IS_DIRECTORY);

    // Get the root node for the directory
    Status = CreateRootNode(File, &RootNode);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to create root node!\n");
        return Status;
    }

    CurrentKey = RootNode->FirstKey;
    EntryCount = 0;

    /* The namespace byte is the on-disk authority for DOS aliases. Count and
     * mark entries in one pass; no filename-shape heuristics are needed.
     */
    for (SearchKey = CurrentKey;
         SearchKey;
         SearchKey = GetNextKey(SearchKey))
    {
        if (!(SearchKey->Entry->Flags & INDEX_ENTRY_END))
        {
            EntryCount++;
            if (GetFileName(SearchKey)->NameType == NAME_TYPE_DOS)
                SearchKey->Flags |= DIR_KEY_8DOT3;
        }
    }

    /* Build an FRN-keyed table so each Win32 entry retains its DOS alias.
     * Enumeration can then obtain the alias in O(1).
     */
    PairTableSize = 8;
    while (EntryCount <= MAXULONG / 2 &&
           PairTableSize < EntryCount * 2 &&
           PairTableSize <= MAXULONG / 2)
    {
        PairTableSize *= 2;
    }

    PairTable = new(PagedPool, TAG_BTREE) SHORT_NAME_PAIR[PairTableSize];
    if (!PairTable)
    {
        DPRINT1("Unable to cache directory short-name pairs.\n");
        return STATUS_SUCCESS;
    }

    RtlZeroMemory(PairTable, PairTableSize * sizeof(*PairTable));
    for (SearchKey = CurrentKey;
         SearchKey;
         SearchKey = GetNextKey(SearchKey))
    {
        if (SearchKey->Entry->Flags & INDEX_ENTRY_END)
            continue;

        FileReference = FileRef(SearchKey);
        PairIndex = HashFileReference(FileReference) & (PairTableSize - 1);
        while (PairTable[PairIndex].Occupied &&
               PairTable[PairIndex].FileReference != FileReference)
        {
            PairIndex = (PairIndex + 1) & (PairTableSize - 1);
        }

        PairTable[PairIndex].Occupied = TRUE;
        PairTable[PairIndex].FileReference = FileReference;
        if (GetFileName(SearchKey)->NameType == NAME_TYPE_DOS)
            PairTable[PairIndex].ShortNameKey = SearchKey;
        else if (GetFileName(SearchKey)->NameType == NAME_TYPE_WIN32)
            PairTable[PairIndex].LongNameKey = SearchKey;
    }

    for (PairIndex = 0; PairIndex < PairTableSize; PairIndex++)
    {
        if (PairTable[PairIndex].LongNameKey)
        {
            PairTable[PairIndex].LongNameKey->ShortNameKey =
                PairTable[PairIndex].ShortNameKey;
        }
    }

    delete[] PairTable;
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
    FileNameString = NtfsMakeCountedUnicodeString(
        FileNameData->Name,
        FileNameData->NameLength * sizeof(WCHAR));

    /* The search entry points uppercase the expression once with this
     * volume's $UpCase table. Use the same table for the candidate name.
     */
    return NtfsIsNameInExpression(NameFilter,
                                  &FileNameString,
                                  IgnoreCase,
                                  DiskVolume->GetUpcaseTable());
}

PBTreeKey
Directory::GetShortNameKey(_In_ PBTreeKey Key)
{
    if (!Key || Key->Entry->Flags & INDEX_ENTRY_END)
        return NULL;

    return Key->ShortNameKey;
}

Directory::Directory(_In_ PVolume DiskVolume)
{
    this->DiskVolume = DiskVolume;
}
