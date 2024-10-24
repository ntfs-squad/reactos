#include "../io/ntfsprocs.h"

static
NTSTATUS
VerifyUpdateSequenceArray(ULONG BytesPerSector,
                          PNTFSRecordHeader Record)
{
    USHORT *USA;
    USHORT USANumber;
    USHORT USACount;
    USHORT *Block;

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

static
NTSTATUS
CreateNode(_In_    PFileRecord File,
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
    IndexBufferSize = BytesPerIndexRecord(File->Volume);

    // Create the new node and first key.
    NewNode = new(PagedPool) BTreeNode();
    CurrentKey = new(PagedPool) BTreeKey();
    NewNode->FirstKey = CurrentKey;
    NodeBuffer = (PIndexBuffer)ExAllocatePoolWithTag(PagedPool,
                                                     IndexBufferSize,
                                                     TAG_NTFS);

    // TODO: Confirm index bitmap has this node marked as in-use
    Status = File->CopyData(IndexAllocationAttribute,
                            (PUCHAR)NodeBuffer,
                            &IndexBufferSize,
                            GetAllocationOffsetFromVCN(File->Volume,
                                                       IndexBufferSize,
                                                       *VCN));

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to create node! Unable to copy data!\n");
        __debugbreak();
        return STATUS_NOT_FOUND;
    }

    // Shamelessly ripped from old driver.
    Status = VerifyUpdateSequenceArray(File->Volume->BytesPerSector,
                                       &NodeBuffer->RecordHeader);

    ASSERT(NT_SUCCESS(Status));
    ASSERT(IndexBufferSize == 0);
    ASSERT(RtlCompareMemory(NodeBuffer->RecordHeader.TypeID, "INDX", 4) == 4);
    ASSERT(NodeBuffer->VCN == *VCN);

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
            CreateNode(File,
                       IndexAllocationAttribute,
                       CurrentKey);

            // TODO: Handle failure
        }

        if (!(CurrentEntry->Flags & INDEX_ENTRY_END))
        {
            // Create the next key
            NextKey = new(PagedPool) BTreeKey();
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

static
NTSTATUS
CreateRootNode(_In_  PFileRecord File,
               _Out_ PBTreeNode *NewRootNode)
{
    NTSTATUS Status;
    PAttribute IndexRootAttribute, IndexAllocationAttribute;
    PIndexRootEx IndexRootData;
    PBTreeNode RootNode;
    PBTreeKey CurrentKey, NextKey;
    PIndexEntry CurrentEntry;
    ULONG_PTR EndOfIndexRootData;

    // This only works on files that are directories.
    ASSERT(File->Header->Flags & FR_IS_DIRECTORY);

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
    RootNode = new(PagedPool) BTreeNode();
    CurrentKey = new(PagedPool) BTreeKey();
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
                return STATUS_NOT_FOUND;
            }

            // TODO: Handle failure.
        }

        if (!(CurrentEntry->Flags & INDEX_ENTRY_END))
        {
            // Create next key
            NextKey = new(PagedPool) BTreeKey();
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

}

NTSTATUS
Directory::LoadDirectory(_In_ PFileRecord File)
{
    NTSTATUS Status;

    Status = CreateRootNode(File, &RootNode);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to load directory!\n");
        return STATUS_NOT_FOUND;
    }

    CurrentKey = RootNode->FirstKey;
    return Status;
}