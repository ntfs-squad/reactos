/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2017 Trevor Thompson
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

/* INCLUDES *****************************************************************/
#include "btree.h"

NTSTATUS
FixupUpdateSequenceArray(PNTFSVolume Volume,
                         PNTFSRecordHeader Record)
{
    USHORT *USA;
    USHORT USANumber;
    USHORT USACount;
    USHORT *Block;

    USA = (USHORT*)((PCHAR)Record + Record->UpdateSequenceOffset);
    USANumber = *(USA++);
    USACount = Record->SizeOfUpdateSequence - 1; /* Exclude the USA Number. */
    Block = (USHORT*)((PCHAR)Record + Volume->BytesPerSector - 2);

    DPRINT("FixupUpdateSequenceArray\n");

    while (USACount)
    {
        if (*Block != USANumber)
        {
            DPRINT1("Mismatch with USA: %u read, %u expected\n" , *Block, USANumber);
            return STATUS_UNSUCCESSFUL;
        }
        *Block = *(USA++);
        Block = (USHORT*)((PCHAR)Block + Volume->BytesPerSector);
        USACount--;
    }

    return STATUS_SUCCESS;
}

// TEMP FUNCTION for diagnostic purposes.
// Prints VCN of every node in an index allocation
VOID
PrintAllVCNs(PNTFSVolume Volume,
             PFileRecord File,
             PAttribute Attr,
             ULONG NodeSize)
{
    ULONGLONG CurrentOffset = 0;
    PINDEX_BUFFER CurrentNode, Buffer;
    ULONGLONG BufferSize = AttributeDataLength(Attr);
    ULONGLONG i;
    NTSTATUS Status;
    int Count = 0;

    if (BufferSize == 0)
    {
        DPRINT1("Index Allocation is empty.\n");
        return;
    }

    Buffer = (PINDEX_BUFFER)ExAllocatePoolWithTag(PagedPool, BufferSize, TAG_NTFS);
    Status = File->CopyData(Attr, (PUCHAR)Buffer, &BufferSize);
    ASSERT(!BufferSize);
    CurrentNode = Buffer;

    // loop through all the nodes
    for (i = 0; i < BufferSize; i += NodeSize)
    {
        Status = FixupUpdateSequenceArray(Volume, &CurrentNode->RecordHeader);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("ERROR: Fixing fixup failed!\n");
            continue;
        }

        DPRINT1("Node #%d, VCN: %I64u\n", Count, CurrentNode->VCN);

        CurrentNode = (PINDEX_BUFFER)((ULONG_PTR)CurrentNode + NodeSize);
        CurrentOffset += NodeSize;
        Count++;
    }

    delete Buffer;
}

/**
* @name GetFileNameAttributeLength
* @implemented
*
* Returns the size of a given FILENAME_ATTRIBUTE, in bytes.
*
* @param FileNameAttribute
* Pointer to a FILENAME_ATTRIBUTE to determine the size of.
*
* @remarks
* The length of a FILENAME_ATTRIBUTE is variable and is dependent on the length of the file name stored at the end.
* This function operates on the FILENAME_ATTRIBUTE proper, so don't try to pass it a PNTFS_ATTR_RECORD.
*/
ULONG GetFileNameAttributeLength(PFileNameEx FileNameAttribute)
{
    ULONG Length = FIELD_OFFSET(FileNameEx, Name) + (FileNameAttribute->NameLength * sizeof(WCHAR));
    return Length;
}

#define NDEBUG

VOID
DumpBTreeKey(PBTree Tree,
             PBTreeKey Key,
             ULONG Number,
             ULONG Depth);

VOID
DestroyBTreeNode(PBTreeFilenameNode Node);

VOID
DumpBTreeNode(PBTree Tree,
              PBTreeFilenameNode Node,
              ULONG Number,
              ULONG Depth)
{
    PBTreeKey CurrentKey;
    ULONG i;
    for (i = 0; i < Depth; i++)
        DbgPrint(" ");
    DbgPrint("Node #%d, Depth %d, has %d key%s", Number, Depth, Node->KeyCount, Node->KeyCount == 1 ? "" : "s");

    if (Node->HasValidVCN)
        DbgPrint(" VCN: %I64u\n", Node->VCN);
    else if (Tree->RootNode == Node)
        DbgPrint(" Index Root");
    else
        DbgPrint(" NOT ASSIGNED VCN YET\n");

    CurrentKey = Node->FirstKey;
    for (i = 0; i < Node->KeyCount; i++)
    {
        DumpBTreeKey(Tree, CurrentKey, i, Depth);
        CurrentKey = CurrentKey->NextKey;
    }
}

/**
* @name DumpBTree
* @implemented
*
* Displays a B-Tree.
*
* @param Tree
* Pointer to the BTree which will be displayed.
*
* @remarks
* Displays a diagnostic summary of a BTree.
*/
VOID
DumpBTree(PBTree Tree)
{
    DbgPrint("BTree @ %p\n", Tree);
    DumpBTreeNode(Tree, Tree->RootNode, 0, 0);
}

VOID
DumpBTreeKey(PBTree Tree,
             PBTreeKey Key,
             ULONG Number,
             ULONG Depth)
{
    ULONG i;
    for (i = 0; i < Depth; i++)
        DbgPrint(" ");
    DbgPrint(" Key #%d", Number);

    if (!(Key->IndexEntry->Flags & NTFS_INDEX_ENTRY_END))
    {
        UNICODE_STRING FileName;
        FileName.Length = Key->IndexEntry->FileName.NameLength * sizeof(WCHAR);
        FileName.MaximumLength = FileName.Length;
        FileName.Buffer = Key->IndexEntry->FileName.Name;
        DbgPrint(" '%wZ'\n", &FileName);
    }
    else
    {
        DbgPrint(" (Dummy Key)\n");
    }

    // Is there a child node?
    if (Key->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
    {
        if (Key->LesserChild)
            DumpBTreeNode(Tree, Key->LesserChild, Number, Depth + 1);
        else
        {
            // This will be an assert once nodes with arbitrary depth are debugged
            DPRINT1("DRIVER ERROR: No Key->LesserChild despite Key->IndexEntry->Flags indicating this is a node!\n");
        }
    }
}


/* FUNCTIONS ****************************************************************/

void
DestroyBTreeKey(PBTreeKey Key)
{
    if (Key->IndexEntry)
        delete Key->IndexEntry;

    if (Key->LesserChild)
        DestroyBTreeNode(Key->LesserChild);

    delete Key;
}

void
DestroyBTreeNode(PBTreeFilenameNode Node)
{
    PBTreeKey NextKey;
    PBTreeKey CurrentKey = Node->FirstKey;
    ULONG i;
    for (i = 0; i < Node->KeyCount; i++)
    {
        ASSERT(CurrentKey);
        NextKey = CurrentKey->NextKey;
        DestroyBTreeKey(CurrentKey);
        CurrentKey = NextKey;
    }

    ASSERT(NextKey == NULL);

    delete Node;
}

/**
* @name DestroyBTree
* @implemented
*
* Destroys a B-Tree.
*
* @param Tree
* Pointer to the BTree which will be destroyed.
*
* @remarks
* Destroys every bit of data stored in the tree.
*/
VOID
DestroyBTree(PBTree Tree)
{
    DestroyBTreeNode(Tree->RootNode);
    delete Tree;
}

// Calculates start of Index Buffer relative to the index allocation, given the node's VCN
ULONGLONG
GetAllocationOffsetFromVCN(PNTFSVolume Volume,
                           ULONG IndexBufferSize,
                           ULONGLONG VCN)
{
    if (IndexBufferSize < BytesPerCluster(Volume))
        return VCN * Volume->BytesPerSector;

    return VCN * BytesPerCluster(Volume);
}

ULONGLONG
GetIndexEntryVCN(PINDEX_ENTRY_ATTRIBUTE IndexEntry)
{
    PULONGLONG Destination = (PULONGLONG)((ULONG_PTR)IndexEntry + IndexEntry->Length - sizeof(ULONGLONG));
    ASSERT(IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE);
    return *Destination;
}

/**
* @name CreateDummyKey
* @implemented
*
* Creates the final BTreeKey for a BTreeFilenameNode. Also creates the associated index entry.
*
* @param HasChildNode
* BOOLEAN to indicate if this key will have a LesserChild.
*
* @return
* The newly-created key.
*/
PBTreeKey
CreateDummyKey(BOOLEAN HasChildNode)
{
    PINDEX_ENTRY_ATTRIBUTE NewIndexEntry;
    PBTreeKey NewDummyKey;

    // Calculate max size of a dummy key
    ULONG EntrySize = ALIGN_UP_BY(FIELD_OFFSET(INDEX_ENTRY_ATTRIBUTE, FileName), 8);
    EntrySize += sizeof(ULONGLONG); // for VCN

    // Create the index entry for the key
    NewIndexEntry = new(NonPagedPool) INDEX_ENTRY_ATTRIBUTE();
    if (!NewIndexEntry)
    {
        DPRINT1("Couldn't allocate memory for dummy key index entry!\n");
        return NULL;
    }

    RtlZeroMemory(NewIndexEntry, EntrySize);

    if (HasChildNode)
    {
        NewIndexEntry->Flags = NTFS_INDEX_ENTRY_NODE | NTFS_INDEX_ENTRY_END;
    }

    else
    {
        NewIndexEntry->Flags = NTFS_INDEX_ENTRY_END;
        EntrySize -= sizeof(ULONGLONG); // no VCN
    }

    NewIndexEntry->Length = EntrySize;

    // Create the key
    NewDummyKey = new(NonPagedPool) BTreeKey();

    if (!NewDummyKey)
    {
        DPRINT1("Unable to allocate dummy key!\n");
        delete NewIndexEntry;
        return NULL;
    }

    RtlZeroMemory(NewDummyKey, sizeof(BTreeKey));
    NewDummyKey->IndexEntry = NewIndexEntry;
    return NewDummyKey;
}

/**
* @name CompareTreeKeys
* @implemented
*
* Compare two BTreeKey's to determine their order in the tree.
*
* @param Key1
* Pointer to a BTreeKey that will be compared.
*
* @param Key2
* Pointer to the other BTreeKey that will be compared.
*
* @param CaseSensitive
* Boolean indicating if the function should operate in case-sensitive mode. This will be TRUE
* if an application created the file with the FILE_FLAG_POSIX_SEMANTICS flag.
*
* @returns
* 0 if the two keys are equal.
* < 0 if key1 is less thank key2
* > 0 if key1 is greater than key2
*
* @remarks
* Any other key is always less than the final (dummy) key in a node. Key1 must not be the dummy node.
*/
LONG
CompareTreeKeys(PBTreeKey Key1, PBTreeKey Key2, BOOLEAN CaseSensitive)
{
    UNICODE_STRING Key1Name, Key2Name;
    LONG Comparison;

    // Key1 must not be the final key (AKA the dummy key)
    ASSERT(!(Key1->IndexEntry->Flags & NTFS_INDEX_ENTRY_END));

    // If Key2 is the "dummy key", key 1 will always come first
    if (Key2->NextKey == NULL)
        return -1;

    Key1Name.Buffer = Key1->IndexEntry->FileName.Name;
    Key1Name.Length = Key1Name.MaximumLength
        = Key1->IndexEntry->FileName.NameLength * sizeof(WCHAR);

    Key2Name.Buffer = Key2->IndexEntry->FileName.Name;
    Key2Name.Length = Key2Name.MaximumLength
        = Key2->IndexEntry->FileName.NameLength * sizeof(WCHAR);

    // Are the two keys the same length?
    if (Key1Name.Length == Key2Name.Length)
        return RtlCompareUnicodeString(&Key1Name, &Key2Name, !CaseSensitive);

    // Is Key1 shorter?
    if (Key1Name.Length < Key2Name.Length)
    {
        // Truncate KeyName2 to be the same length as KeyName1
        Key2Name.Length = Key1Name.Length;

        // Compare the names of the same length
        Comparison = RtlCompareUnicodeString(&Key1Name, &Key2Name, !CaseSensitive);

        // If the truncated names are the same length, the shorter one comes first
        if (Comparison == 0)
            return -1;
    }
    else
    {
        // Key2 is shorter
        // Truncate KeyName1 to be the same length as KeyName2
        Key1Name.Length = Key2Name.Length;

        // Compare the names of the same length
        Comparison = RtlCompareUnicodeString(&Key1Name, &Key2Name, !CaseSensitive);

        // If the truncated names are the same length, the shorter one comes first
        if (Comparison == 0)
            return 1;
    }

    return Comparison;
}

/**
* @name CountBTreeKeys
* @implemented
*
* Counts the number of linked B-Tree keys, starting with FirstKey.
*
* @param FirstKey
* Pointer to a BTreeKey that will be the first key to be counted.
*
* @return
* The number of keys in a linked-list, including FirstKey and the final dummy key.
*/
ULONG
CountBTreeKeys(PBTreeKey FirstKey)
{
    ULONG Count = 0;
    PBTreeKey CurrentKey = FirstKey;

    while (CurrentKey)
    {
        Count++;
        CurrentKey = CurrentKey->NextKey;
    }

    return Count;
}

/**
* @name GetSizeOfIndexEntries
* @implemented
*
* Sums the size of each index entry in every key in a B-Tree node.
*
* @param Node
* Pointer to a BTreeFilenameNode. The size of this node's index entries will be returned.
*
* @returns
* The sum of the sizes of every index entry for each key in the B-Tree node.
*
* @remarks
* Gets only the size of the index entries; doesn't include the size of any headers that would be added to an index record.
*/
ULONG
GetSizeOfIndexEntries(PBTreeFilenameNode Node)
{
    // Start summing the total size of this node's entries
    ULONG NodeSize = 0;

    // Walk through the list of Node Entries
    PBTreeKey CurrentKey = Node->FirstKey;
    ULONG i;

    for (i = 0; i < Node->KeyCount; i++)
    {
        ASSERT(CurrentKey->IndexEntry->Length != 0);
        // Add the length of the current node
        NodeSize += CurrentKey->IndexEntry->Length;
        CurrentKey = CurrentKey->NextKey;
    }

    return NodeSize;
}

PBTreeFilenameNode
CreateBTreeNodeFromIndexNode(PNTFSVolume Volume,
                             PFileRecord File,
                             IndexRootEx* IndexRoot,
                             PAttribute IndexAllocationAttribute,
                             PINDEX_ENTRY_ATTRIBUTE NodeEntry)
{
    PBTreeFilenameNode NewNode;
    PINDEX_ENTRY_ATTRIBUTE CurrentNodeEntry;
    PINDEX_ENTRY_ATTRIBUTE FirstNodeEntry;
    ULONG CurrentEntryOffset;
    PINDEX_BUFFER NodeBuffer;
    ULONGLONG IndexBufferSize;
    PULONGLONG VCN;
    PBTreeKey CurrentKey;
    NTSTATUS Status;
    ULONGLONG IndexNodeOffset;

    ASSERT(IndexAllocationAttribute);
    CurrentEntryOffset = 0;
    IndexBufferSize = BytesPerIndexRecord(Volume);

    // Get the node number from the end of the node entry
    VCN = (PULONGLONG)((ULONG_PTR)NodeEntry + NodeEntry->Length - sizeof(ULONGLONG));

    // Create the new tree node
    NewNode = new(NonPagedPool) BTreeFilenameNode();
    if (!NewNode)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new filename node.\n");
        return NULL;
    }
    RtlZeroMemory(NewNode, sizeof(BTreeFilenameNode));

    // Create the first key
    CurrentKey = new(NonPagedPool) BTreeKey();
    if (!CurrentKey)
    {
        DPRINT1("ERROR: Failed to allocate memory for key!\n");
        delete NewNode;
        return NULL;
    }
    RtlZeroMemory(CurrentKey, sizeof(BTreeKey));
    NewNode->FirstKey = CurrentKey;

    // Allocate memory for the node buffer
    NodeBuffer = (PINDEX_BUFFER)ExAllocatePoolWithTag(NonPagedPool, IndexBufferSize, TAG_NTFS);
    if (!NodeBuffer)
    {
        DPRINT1("ERROR: Couldn't allocate memory for node buffer!\n");
        delete CurrentKey;
        delete NewNode;
        return NULL;
    }

    // Calculate offset into index allocation
    IndexNodeOffset = GetAllocationOffsetFromVCN(Volume, IndexBufferSize, *VCN);

    // TODO: Confirm index bitmap has this node marked as in-use

    // Read the node
    // BytesRead = ReadAttribute(Volume,
    //                           IndexAllocationAttributeCtx,
    //                           IndexNodeOffset,
    //                           (PCHAR)NodeBuffer,
    //                           IndexBufferSize);

    File->CopyData(IndexAllocationAttribute, (PUCHAR)NodeBuffer, &IndexBufferSize);

    ASSERT(IndexBufferSize == 0);
    ASSERT(RtlCompareMemory(NodeBuffer->RecordHeader.TypeID, "INDX", 4) == 4);
    ASSERT(NodeBuffer->VCN == *VCN);

    // Apply the fixup array to the node buffer
    Status = FixupUpdateSequenceArray(Volume, &NodeBuffer->RecordHeader);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("ERROR: Couldn't apply fixup array to index node buffer!\n");
        delete NodeBuffer;
        delete CurrentKey;
        delete NewNode;
        return NULL;
    }

    // Walk through the index and create keys for all the entries
    FirstNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)(&NodeBuffer->IndexHeader)
                                               + NodeBuffer->IndexHeader.IndexOffset);
    CurrentNodeEntry = FirstNodeEntry;
    while (CurrentEntryOffset < NodeBuffer->IndexHeader.TotalIndexSize)
    {
        // Allocate memory for the current entry
        CurrentKey->IndexEntry = (PINDEX_ENTRY_ATTRIBUTE)ExAllocatePoolWithTag(NonPagedPool,
                                                                               CurrentNodeEntry->Length,
                                                                               TAG_NTFS);
        if (!CurrentKey->IndexEntry)
        {
            DPRINT1("ERROR: Couldn't allocate memory for next key!\n");
            DestroyBTreeNode(NewNode);
            delete NodeBuffer;
            return NULL;
        }

        NewNode->KeyCount++;

        // If this isn't the last entry
        if (!(CurrentNodeEntry->Flags & NTFS_INDEX_ENTRY_END))
        {
            // Create the next key
            PBTreeKey NextKey = new(NonPagedPool) BTreeKey();
            if (!NextKey)
            {
                DPRINT1("ERROR: Couldn't allocate memory for next key!\n");
                DestroyBTreeNode(NewNode);
                delete NodeBuffer;
                return NULL;
            }
            RtlZeroMemory(NextKey, sizeof(BTreeKey));

            // Add NextKey to the end of the list
            CurrentKey->NextKey = (PBTreeKey)NextKey;

            // Copy the current entry to its key
            RtlCopyMemory(CurrentKey->IndexEntry, CurrentNodeEntry, CurrentNodeEntry->Length);

            // See if the current key has a sub-node
            if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
            {
                CurrentKey->LesserChild = CreateBTreeNodeFromIndexNode(Volume,
                                                                       File,
                                                                       IndexRoot,
                                                                       IndexAllocationAttribute,
                                                                       CurrentKey->IndexEntry);
            }

            CurrentKey = NextKey;
        }
        else
        {
            // Copy the final entry to its key
            RtlCopyMemory(CurrentKey->IndexEntry, CurrentNodeEntry, CurrentNodeEntry->Length);
            CurrentKey->NextKey = NULL;

            // See if the current key has a sub-node
            if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
            {
                CurrentKey->LesserChild = CreateBTreeNodeFromIndexNode(Volume,
                                                                       File,
                                                                       IndexRoot,
                                                                       IndexAllocationAttribute,
                                                                       CurrentKey->IndexEntry);
            }

            break;
        }

        // Advance to the next entry
        CurrentEntryOffset += CurrentNodeEntry->Length;
        CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->Length);
    }

    NewNode->VCN = *VCN;
    NewNode->HasValidVCN = TRUE;

    delete NodeBuffer;

    return NewNode;
}

/**
* @name CreateBTreeFromIndex
* @implemented
*
* Parse an index and create a B-Tree in memory from it.
*
* @param IndexRootContext
* Pointer to an NTFS_ATTR_CONTEXT that describes the location of the index root attribute.
*
* @param NewTree
* Pointer to a PBTree that will receive the pointer to a newly-created B-Tree.
*
* @returns
* STATUS_SUCCESS on success.
* STATUS_INSUFFICIENT_RESOURCES if an allocation fails.
*
* @remarks
* Allocates memory for the entire tree. Caller is responsible for destroying the tree with DestroyBTree().
*/
NTSTATUS
CreateBTreeFromIndex(PNTFSVolume Volume,
                     PFileRecord FileRecordWithIndex,
                     /*PCWSTR IndexName,*/
                     // PNTFS_ATTR_CONTEXT IndexRootContext,
                     PAttribute IndexRootAttribute,
                     IndexRootEx* IndexRoot,
                     PBTree *NewTree)
{
    PINDEX_ENTRY_ATTRIBUTE CurrentNodeEntry;
    PBTree Tree = new(NonPagedPool) BTree();
    PBTreeFilenameNode RootNode = new(NonPagedPool) BTreeFilenameNode();
    PBTreeKey CurrentKey = new(NonPagedPool) BTreeKey();
    ULONG CurrentOffset = IndexRoot->Header.IndexOffset;
    PAttribute IndexAllocationAttribute;
    NTSTATUS Status;

    DPRINT("CreateBTreeFromIndex(%p, %p)\n", IndexRoot, NewTree);

    if (!Tree || !RootNode || !CurrentKey)
    {
        DPRINT1("Couldn't allocate enough memory for B-Tree!\n");
        if (Tree)
            delete Tree;
        if (CurrentKey)
            delete CurrentKey;
        if (RootNode)
            delete RootNode;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Tree, sizeof(BTree));
    RtlZeroMemory(RootNode, sizeof(BTreeFilenameNode));
    RtlZeroMemory(CurrentKey, sizeof(BTreeKey));

    // See if the file record has an attribute allocation
    IndexAllocationAttribute = FileRecordWithIndex->GetAttribute(AttributeType::IndexAllocation, L"$I30");

    // Setup the Tree
    RootNode->FirstKey = CurrentKey;
    Tree->RootNode = RootNode;

    // Make sure we won't try reading past the attribute-end
    if (FIELD_OFFSET(IndexRootEx, Header) + IndexRoot->Header.TotalIndexSize > IndexRootAttribute->Resident.DataLength)
    {
        DPRINT1("Filesystem corruption detected!\n");
        DestroyBTree(Tree);
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Cleanup;
    }

    // Start at the first node entry
    CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)IndexRoot
                                                + FIELD_OFFSET(IndexRootEx, Header)
                                                + IndexRoot->Header.IndexOffset);

    // Create a key for each entry in the node
    while (CurrentOffset < IndexRoot->Header.TotalIndexSize)
    {
        // Allocate memory for the current entry
        CurrentKey->IndexEntry = (PINDEX_ENTRY_ATTRIBUTE)ExAllocatePoolWithTag(NonPagedPool,
                                                                               CurrentNodeEntry->Length,
                                                                               TAG_NTFS);
        if (!CurrentKey->IndexEntry)
        {
            DPRINT1("ERROR: Couldn't allocate memory for next key!\n");
            DestroyBTree(Tree);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Cleanup;
        }

        RootNode->KeyCount++;

        // If this isn't the last entry
        if (!(CurrentNodeEntry->Flags & NTFS_INDEX_ENTRY_END))
        {
            // Create the next key
            PBTreeKey NextKey = new(NonPagedPool) BTreeKey();
            if (!NextKey)
            {
                DPRINT1("ERROR: Couldn't allocate memory for next key!\n");
                DestroyBTree(Tree);
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Cleanup;
            }

            RtlZeroMemory(NextKey, sizeof(BTreeKey));

            // Add NextKey to the end of the list
            CurrentKey->NextKey = NextKey;

            // Copy the current entry to its key
            RtlCopyMemory(CurrentKey->IndexEntry, CurrentNodeEntry, CurrentNodeEntry->Length);

            // Does this key have a sub-node?
            if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
            {
                // Create the child node
                CurrentKey->LesserChild = CreateBTreeNodeFromIndexNode(Volume,
                                                                       FileRecordWithIndex,
                                                                       IndexRoot,
                                                                       IndexRootAttribute,
                                                                       CurrentKey->IndexEntry);
                if (!CurrentKey->LesserChild)
                {
                    DPRINT1("ERROR: Couldn't create child node!\n");
                    DestroyBTree(Tree);
                    Status = STATUS_NOT_IMPLEMENTED;
                    goto Cleanup;
                }
            }

            // Advance to the next entry
            CurrentOffset += CurrentNodeEntry->Length;
            CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->Length);
            CurrentKey = NextKey;
        }
        else
        {
            // Copy the final entry to its key
            RtlCopyMemory(CurrentKey->IndexEntry, CurrentNodeEntry, CurrentNodeEntry->Length);
            CurrentKey->NextKey = NULL;

            // Does this key have a sub-node?
            if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
            {
                // Create the child node
                CurrentKey->LesserChild = CreateBTreeNodeFromIndexNode(Volume,
                                                                       FileRecordWithIndex,
                                                                       IndexRoot,
                                                                       IndexRootAttribute,
                                                                       CurrentKey->IndexEntry);
                if (!CurrentKey->LesserChild)
                {
                    DPRINT1("ERROR: Couldn't create child node!\n");
                    DestroyBTree(Tree);
                    Status = STATUS_NOT_IMPLEMENTED;
                    goto Cleanup;
                }
            }

            break;
        }
    }

    *NewTree = Tree;
    Status = STATUS_SUCCESS;

Cleanup:
    if (IndexAllocationAttribute)
        delete IndexAllocationAttribute;

    return Status;
}

/**
* @name CreateIndexRootFromBTree
* @implemented
*
* Parse a B-Tree in memory and convert it into an index that can be written to disk.
*
* @param Volume
* Pointer to the PNTFSVolume of the target drive.
*
* @param Tree
* Pointer to a BTree that describes the index to be written.
*
* @param MaxIndexSize
* Describes how large the index can be before it will take too much space in the file record.
* This is strictly the sum of the sizes of all index entries; it does not include the space
* required by the index root header (IndexRootEx), since that size will be constant.
*
* After reaching MaxIndexSize, an index can no longer be represented with just an index root
* attribute, and will require an index allocation and $I30 bitmap (TODO).
*
* @param IndexRoot
* Pointer to a IndexRootEx* that will receive a pointer to the newly-created index.
*
* @param Length
* Pointer to a ULONG which will receive the length of the new index root.
*
* @returns
* STATUS_SUCCESS on success.
* STATUS_INSUFFICIENT_RESOURCES if an allocation fails.
* STATUS_NOT_IMPLEMENTED if the new index can't fit within MaxIndexSize.
*
* @remarks
* If the function succeeds, it's the caller's responsibility to free IndexRoot with delete.
*/
NTSTATUS
CreateIndexRootFromBTree(PNTFSVolume Volume,
                         PBTree Tree,
                         ULONG MaxIndexSize,
                         PIndexRootEx *IndexRoot,
                         ULONG *Length)
{
    ULONG i;
    PBTreeKey CurrentKey;
    PINDEX_ENTRY_ATTRIBUTE CurrentNodeEntry;
    IndexRootEx* NewIndexRoot = (PIndexRootEx)ExAllocatePoolWithTag(NonPagedPool,
                                                                    Volume->FileRecordSize,
                                                                    TAG_NTFS);

    DPRINT("CreateIndexRootFromBTree(%p, %p, 0x%lx, %p, %p)\n", Volume, Tree, MaxIndexSize, IndexRoot, Length);

#ifdef NTFS_DEBUG
    DumpBTree(Tree);
#endif

    if (!NewIndexRoot)
    {
        DPRINT1("Failed to allocate memory for Index Root!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Setup the new index root
    RtlZeroMemory(NewIndexRoot, Volume->FileRecordSize);

    NewIndexRoot->AttributeType = FileName;
    NewIndexRoot->CollationRule = COLLATION_FILE_NAME;
    NewIndexRoot->BytesPerIndexRec = BytesPerIndexRecord(Volume);
    // If Bytes per index record is less than cluster size, clusters per index record becomes sectors per index
    if (NewIndexRoot->BytesPerIndexRec < BytesPerCluster(Volume))
        NewIndexRoot->ClusPerIndexRec = NewIndexRoot->BytesPerIndexRec / Volume->BytesPerSector;
    else
        NewIndexRoot->ClusPerIndexRec = NewIndexRoot->BytesPerIndexRec / BytesPerCluster(Volume);

    // Setup the Index node header
    NewIndexRoot->Header.IndexOffset = sizeof(IndexNodeHeader);
    NewIndexRoot->Header.Flags = INDEX_ROOT_SMALL;

    // Start summing the total size of this node's entries
    NewIndexRoot->Header.TotalIndexSize = NewIndexRoot->Header.IndexOffset;

    // Setup each Node Entry
    CurrentKey = Tree->RootNode->FirstKey;
    CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)NewIndexRoot
                                                + FIELD_OFFSET(IndexRootEx, Header)
                                                + NewIndexRoot->Header.IndexOffset);

    for (i = 0; i < Tree->RootNode->KeyCount; i++)
    {
        // Would adding the current entry to the index increase the index size beyond the limit we've set?
        ULONG IndexSize = NewIndexRoot->Header.TotalIndexSize - NewIndexRoot->Header.IndexOffset + CurrentKey->IndexEntry->Length;
        if (IndexSize > MaxIndexSize)
        {
            DPRINT1("TODO: Adding file would require creating an attribute list!\n");
            delete NewIndexRoot;
            return STATUS_NOT_IMPLEMENTED;
        }

        ASSERT(CurrentKey->IndexEntry->Length != 0);

        // Copy the index entry
        RtlCopyMemory(CurrentNodeEntry, CurrentKey->IndexEntry, CurrentKey->IndexEntry->Length);

        DPRINT1("Index Node Entry Stream Length: %u\nIndex Node Entry Length: %u\n",
                CurrentNodeEntry->KeyLength,
                CurrentNodeEntry->Length);

        // Does the current key have any sub-nodes?
        if (CurrentKey->LesserChild)
            NewIndexRoot->Header.Flags = INDEX_ROOT_LARGE;

        // Add Length of Current Entry to Total Size of Entries
        NewIndexRoot->Header.TotalIndexSize += CurrentKey->IndexEntry->Length;

        // Go to the next node entry
        CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->Length);

        CurrentKey = CurrentKey->NextKey;
    }

    NewIndexRoot->Header.AllocatedSize = NewIndexRoot->Header.TotalIndexSize;

    *IndexRoot = NewIndexRoot;
    *Length = NewIndexRoot->Header.AllocatedSize + FIELD_OFFSET(IndexRootEx, Header);

    return STATUS_SUCCESS;
}

PBTreeKey
CreateBTreeKeyFromFilename(ULONGLONG FileReference, PFileNameEx FileNameAttribute)
{
    PBTreeKey NewKey;
    ULONG AttributeSize = GetFileNameAttributeLength(FileNameAttribute);
    ULONG EntrySize = ALIGN_UP_BY(AttributeSize + FIELD_OFFSET(INDEX_ENTRY_ATTRIBUTE, FileName), 8);

    // Create a new Index Entry for the file
    PINDEX_ENTRY_ATTRIBUTE NewEntry = (PINDEX_ENTRY_ATTRIBUTE)ExAllocatePoolWithTag(NonPagedPool,
                                                                                    EntrySize,
                                                                                    TAG_NTFS);
    if (!NewEntry)
    {
        DPRINT1("ERROR: Failed to allocate memory for Index Entry!\n");
        return NULL;
    }

    // Setup the Index Entry
    RtlZeroMemory(NewEntry, EntrySize);
    NewEntry->Data.Directory.IndexedFile = FileReference;
    NewEntry->Length = EntrySize;
    NewEntry->KeyLength = AttributeSize;

    // Copy the FileNameAttribute
    RtlCopyMemory(&NewEntry->FileName, FileNameAttribute, AttributeSize);

    // Setup the New Key
    NewKey = new(NonPagedPool) BTreeKey();
    if (!NewKey)
    {
        DPRINT1("ERROR: Failed to allocate memory for new key!\n");
        delete NewEntry;
        return NULL;
    }
    NewKey->IndexEntry = NewEntry;
    NewKey->NextKey = NULL;

    return NewKey;
}
