#include "ntfspch.h"

#define GetFileNameAttributeLength(FileNameAttribute) \
FIELD_OFFSET(FileNameEx, Name) + (FileNameAttribute->NameLength * sizeof(WCHAR))

#define INDEX_NODE_SMALL 0x0
#define INDEX_NODE_LARGE 0x1

PBTreeKey
Directory::CreateDummyKey(BOOLEAN HasChildNode)
{
    PIndexEntry NewEntry;
    PBTreeKey NewKey;

    // Calculate max size of a dummy key
    ULONG EntrySize = ALIGN_UP_BY(FIELD_OFFSET(IndexEntry, Data), 8);
    EntrySize += sizeof(ULONGLONG); // for VCN

    // Create the index entry for the key
    NewEntry = (PIndexEntry)ExAllocatePoolWithTag(PagedPool, EntrySize, TAG_NTFS);
    if (!NewEntry)
    {
        DPRINT1("Couldn't allocate memory for dummy key index entry!\n");
        return NULL;
    }
    RtlZeroMemory(NewEntry, EntrySize);

    if (HasChildNode)
    {
        NewEntry->Flags = INDEX_ENTRY_NODE | INDEX_ENTRY_END;
    }

    else
    {
        NewEntry->Flags = INDEX_ENTRY_END;
        EntrySize -= sizeof(ULONGLONG); // no VCN
    }

    NewEntry->EntryLength = EntrySize;

    // Create the key
    NewKey = new(PagedPool) BTreeKey();
    if (!NewKey)
    {
        DPRINT1("Unable to allocate dummy key!\n");
        delete NewKey;
        return NULL;
    }

    RtlZeroMemory(NewKey, sizeof(BTreeKey));
    NewKey->Entry = NewEntry;
    return NewKey;
}

NTSTATUS
Directory::CreateEmptyBTree(PBTree* NewTree)
{
    PBTree Tree = new(NonPagedPool) BTree;
    PBTreeNode RootNode = new(NonPagedPool) BTreeNode;
    PBTreeKey DummyKey;

    DPRINT1("CreateEmptyBTree(%p) called\n", NewTree);

    if (!Tree || !RootNode)
    {
        DPRINT1("Couldn't allocate enough memory for B-Tree!\n");
        if (Tree)
            ExFreePoolWithTag(Tree, TAG_NTFS);
        if (RootNode)
            ExFreePoolWithTag(RootNode, TAG_NTFS);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Create the dummy key
    DummyKey = CreateDummyKey(FALSE);
    if (!DummyKey)
    {
        DPRINT1("ERROR: Failed to create dummy key!\n");
        ExFreePoolWithTag(Tree, TAG_NTFS);
        ExFreePoolWithTag(RootNode, TAG_NTFS);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Tree, sizeof(BTree));
    RtlZeroMemory(RootNode, sizeof(BTreeNode));

    // Setup the Tree
    RootNode->FirstKey = DummyKey;
    // RootNode->KeyCount = 1;
    // RootNode->DiskNeedsUpdating = TRUE;
    Tree->RootNode = RootNode;

    *NewTree = Tree;

    // Memory will be freed when DestroyBTree() is called

    return STATUS_SUCCESS;
}

PBTreeKey
Directory::CreateBTreeKeyFromFilename(ULONGLONG FileReference,
                                      PFileNameEx FileNameAttribute)
{
    PBTreeKey NewKey;
    ULONG AttributeSize = GetFileNameAttributeLength(FileNameAttribute);
    ULONG EntrySize = ALIGN_UP_BY(AttributeSize + FIELD_OFFSET(IndexEntry, IndexStream), 8);

    // Create a new Index Entry for the file
    PIndexEntry NewEntry = (PIndexEntry)ExAllocatePoolWithTag(NonPagedPool, EntrySize, TAG_NTFS);
    if (!NewEntry)
    {
        DPRINT1("ERROR: Failed to allocate memory for Index Entry!\n");
        return NULL;
    }

    // Setup the Index Entry
    RtlZeroMemory(NewEntry, EntrySize);
    NewEntry->Data.Directory.IndexedFile = FileReference;
    NewEntry->EntryLength = EntrySize;
    NewEntry->StreamLength = AttributeSize;

    // Copy the FileNameAttribute
    RtlCopyMemory(&NewEntry->IndexStream, FileNameAttribute, AttributeSize);

    // Setup the New Key
    NewKey = new(NonPagedPool) BTreeKey();
    if (!NewKey)
    {
        DPRINT1("ERROR: Failed to allocate memory for new key!\n");
        ExFreePoolWithTag(NewEntry, TAG_NTFS);
        return NULL;
    }
    NewKey->Entry = NewEntry;
    NewKey->NextKey = NULL;

    return NewKey;
}

NTSTATUS
Directory::CreateIndexBufferFromBTreeNode(PBTreeNode Node,
                                          ULONG BufferSize,
                                          BOOLEAN HasChildren,
                                          PIndexBuffer TargetIndexBuffer)
{
    PBTreeKey CurrentKey;
    PIndexEntry CurrentNodeEntry;
    NTSTATUS Status;

    // TODO: Fix magic, do math
    RtlZeroMemory(TargetIndexBuffer, BufferSize);
    TargetIndexBuffer->RecordHeader.TypeID[0] = 'I';
    TargetIndexBuffer->RecordHeader.TypeID[1] = 'N';
    TargetIndexBuffer->RecordHeader.TypeID[2] = 'D';
    TargetIndexBuffer->RecordHeader.TypeID[3] = 'X';
    TargetIndexBuffer->RecordHeader.UpdateSequenceOffset = 0x28;
    TargetIndexBuffer->RecordHeader.SizeOfUpdateSequence = 9;

    // TODO: Check bitmap for VCN
    // ASSERT(Node->HasValidVCN);
    TargetIndexBuffer->VCN = Node->VCN;

    // Windows seems to alternate between using 0x28 and 0x40 for the first entry offset of each index buffer.
    // Interestingly, neither Windows nor chkdsk seem to mind if we just use 0x28 for every index record.
    TargetIndexBuffer->IndexHeader.IndexOffset = 0x28;
    TargetIndexBuffer->IndexHeader.AllocatedSize = BufferSize - FIELD_OFFSET(IndexBuffer, IndexHeader);

    // Start summing the total size of this node's entries
    TargetIndexBuffer->IndexHeader.TotalIndexSize = TargetIndexBuffer->IndexHeader.IndexOffset;

    CurrentKey = Node->FirstKey;
    CurrentNodeEntry = (PIndexEntry)((ULONG_PTR)&(TargetIndexBuffer->IndexHeader)
                                                + TargetIndexBuffer->IndexHeader.IndexOffset);
    while (CurrentKey)
    {
        // Would adding the current entry to the index increase the node size beyond the allocation size?
        ULONG IndexSize = FIELD_OFFSET(IndexBuffer, IndexHeader)
            + TargetIndexBuffer->IndexHeader.TotalIndexSize
            + CurrentNodeEntry->EntryLength;
        if (IndexSize > BufferSize)
        {
            DPRINT1("TODO: Adding file would require creating a new node!\n");
            return STATUS_NOT_IMPLEMENTED;
        }

        ASSERT(CurrentKey->Entry->EntryLength != 0);

        // Copy the index entry
        RtlCopyMemory(CurrentNodeEntry, CurrentKey->Entry, CurrentKey->Entry->EntryLength);

        DPRINT("Index Node Entry Stream Length: %u\nIndex Node Entry Length: %u\n",
               CurrentNodeEntry->StreamLength,
               CurrentNodeEntry->EntryLength);

        // Add Length of Current Entry to Total Size of Entries
        TargetIndexBuffer->IndexHeader.TotalIndexSize += CurrentNodeEntry->EntryLength;

        // Check for child nodes
        if (HasChildren)
            TargetIndexBuffer->IndexHeader.Flags = INDEX_NODE_LARGE;

        // Go to the next node entry
        CurrentNodeEntry = (PIndexEntry)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->EntryLength);
        CurrentKey = CurrentKey->NextKey;
    }

    // TODO: Adapt this. Probably belongs in LFS.
    // Status = AddFixupArray(DeviceExt, &IndexBuffer->Ntfs);
    Status = STATUS_SUCCESS;

    return Status;
}

ULONG
Directory::GetSizeOfIndexEntries(PBTreeNode Node)
{
    // Start summing the total size of this node's entries
    ULONG NodeSize = 0;

    // Walk through the list of Node Entries
    PBTreeKey CurrentKey = Node->FirstKey;
    while (CurrentKey)
    {
        ASSERT(CurrentKey->Entry->EntryLength != 0);

        // Add the length of the current node
        NodeSize += CurrentKey->Entry->EntryLength;
        CurrentKey = CurrentKey->NextKey;
    }

    return NodeSize;
}

VOID
Directory::SetIndexEntryVCN(PIndexEntry IndexEntry, ULONGLONG VCN)
{
    PULONGLONG Destination = (PULONGLONG)((ULONG_PTR)IndexEntry + IndexEntry->EntryLength - sizeof(ULONGLONG));

    ASSERT(IndexEntry->Flags & INDEX_ENTRY_NODE);

    *Destination = VCN;
}

LONG
Directory::CompareTreeKeys(PBTreeKey Key1, PBTreeKey Key2, BOOLEAN CaseSensitive)
{
    UNICODE_STRING Key1Name, Key2Name;
    LONG Comparison;

    // Key1 must not be the final key (AKA the dummy key)
    ASSERT(!(Key1->Entry->Flags & INDEX_ENTRY_END));

    // If Key2 is the "dummy key", key 1 will always come first
    if (Key2->NextKey == NULL)
        return -1;

    Key1Name.Buffer = GetFileName(Key1)->Name;
    Key1Name.Length = GetFileName(Key1)->NameLength * sizeof(WCHAR);
    Key1Name.MaximumLength = Key1Name.Length;

    Key2Name.Buffer = GetFileName(Key2)->Name;
    Key2Name.Length = GetFileName(Key2)->NameLength * sizeof(WCHAR);
    Key2Name.MaximumLength = Key2Name.Length;

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

NTSTATUS
Directory::DemoteBTreeRoot()
{
    PBTreeNode NewSubNode, NewIndexRoot;
    PBTreeKey DummyKey;

    DPRINT("Collapsing Index Root into sub-node.\n");

#ifndef NDEBUG
    DumpFileTree();
#endif

    // Create a new node that will hold the keys currently in index root
    NewSubNode = new(NonPagedPool) BTreeNode();
    if (!NewSubNode)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new sub-node.\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(NewSubNode, sizeof(BTreeNode));

    // Copy the applicable data from the old index root node
    // NewSubNode->KeyCount = RootNode->KeyCount;
    NewSubNode->FirstKey = RootNode->FirstKey;
    // NewSubNode->DiskNeedsUpdating = TRUE;

    // Create a new dummy key, and make the new node it's child
    DummyKey = CreateDummyKey(TRUE);
    if (!DummyKey)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new root node.\n");
        ExFreePoolWithTag(NewSubNode, TAG_NTFS);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Make the new node a child of the dummy key
    DummyKey->ChildNode = NewSubNode;

    // Create a new index root node
    NewIndexRoot = new(NonPagedPool) BTreeNode();
    if (!NewIndexRoot)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new index root.\n");
        delete NewSubNode;
        delete DummyKey;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(NewIndexRoot, sizeof(BTreeNode));

    // Insert the dummy key into the new node
    NewIndexRoot->FirstKey = DummyKey;
    // NewIndexRoot->KeyCount = 1;
    // NewIndexRoot->DiskNeedsUpdating = TRUE;

    // Make the new node the Tree's root node
    RootNode = NewIndexRoot;

#ifndef NDEBUG
    DumpFileTree();
#endif

    return STATUS_SUCCESS;
}

NTSTATUS
Directory::SplitBTreeNode(PBTreeNode Node,
                          PBTreeKey *MedianKey,
                          PBTreeNode *NewRightHandSibling,
                          BOOLEAN CaseSensitive)
{
    ULONG MedianKeyIndex;
    PBTreeKey LastKeyBeforeMedian, FirstKeyAfterMedian;
    ULONG HalfSize;
    ULONG SizeSum;

    DPRINT("SplitBTreeNode(%p, %p, %p, %s) called\n",
            Node,
            MedianKey,
            NewRightHandSibling,
            CaseSensitive ? "TRUE" : "FALSE");

#ifndef NDEBUG
    // DumpBTreeNode(Tree, Node, 0, 0);
#endif

    // Create the right hand sibling
    *NewRightHandSibling = new(NonPagedPool) BTreeNode();
    if (*NewRightHandSibling == NULL)
    {
        DPRINT1("Error: Failed to allocate memory for right hand sibling!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(*NewRightHandSibling, sizeof(BTreeNode));
    // (*NewRightHandSibling)->DiskNeedsUpdating = TRUE;


    // Find the last key before the median

    // This is roughly how NTFS-3G calculates median, and it's not congruent with what Windows does:
    /*
    // find the median key index
    MedianKeyIndex = (Node->KeyCount + 1) / 2;
    MedianKeyIndex--;

    LastKeyBeforeMedian = Node->FirstKey;
    for (i = 0; i < MedianKeyIndex - 1; i++)
        LastKeyBeforeMedian = LastKeyBeforeMedian->NextKey;*/

    // The method we'll use is a little bit closer to how Windows determines the median but it's not identical.
    // What Windows does is actually more complicated than this, I think because Windows allocates more slack space to Odd-numbered
    // Index Records, leaving less room for index entries in these records (I haven't discovered why this is done).
    // (Neither Windows nor chkdsk complain if we choose a different median than Windows would have chosen, as our median will be in the ballpark)

    // Use size to locate the median key / index
    LastKeyBeforeMedian = Node->FirstKey;
    MedianKeyIndex = 0;
    HalfSize = 2016; // half the allocated size after subtracting the first index entry offset (TODO: MATH)
    SizeSum = 0;
    while (LastKeyBeforeMedian)
    {
        SizeSum += LastKeyBeforeMedian->Entry->EntryLength;

        if (SizeSum > HalfSize)
            break;

        MedianKeyIndex++;
        LastKeyBeforeMedian = LastKeyBeforeMedian->NextKey;
    }

    // Now we can get the median key and the key that follows it
    *MedianKey = LastKeyBeforeMedian->NextKey;
    FirstKeyAfterMedian = (*MedianKey)->NextKey;

    // DPRINT1("%lu keys, %lu median\n", Node->KeyCount, MedianKeyIndex);
    DPRINT1("\t\tMedian: %.*S\n", GetFileName((*MedianKey))->NameLength, GetFileName((*MedianKey))->Name);

    // "Node" will be the left hand sibling after the split, containing all keys prior to the median key

    // We need to create a dummy pointer at the end of the LHS. The dummy's child will be the median's child.
    LastKeyBeforeMedian->NextKey = CreateDummyKey(BooleanFlagOn((*MedianKey)->Entry->Flags, INDEX_ENTRY_NODE));
    if (LastKeyBeforeMedian->NextKey == NULL)
    {
        DPRINT1("Error: Couldn't allocate dummy key!\n");
        LastKeyBeforeMedian->NextKey = *MedianKey;
        delete *NewRightHandSibling;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Did the median key have a child node?
    if ((*MedianKey)->Entry->Flags & INDEX_ENTRY_NODE)
    {
        // Set the child of the new dummy key
        LastKeyBeforeMedian->NextKey->ChildNode = (*MedianKey)->ChildNode;

        // Give the dummy key's index entry the same sub-node VCN the median
        SetIndexEntryVCN(LastKeyBeforeMedian->NextKey->Entry, GetIndexEntryVCN((*MedianKey)));
    }
    else
    {
        // Median key didn't have a child node, but it will. Create a new index entry large enough to store a VCN.
        PIndexEntry NewIndexEntry = (PIndexEntry)ExAllocatePoolWithTag(NonPagedPool,
                                                                       (*MedianKey)->Entry->EntryLength + sizeof(ULONGLONG),
                                                                       TAG_NTFS);
        if (!NewIndexEntry)
        {
            DPRINT1("Unable to allocate memory for new index entry!\n");
            LastKeyBeforeMedian->NextKey = *MedianKey;
            delete *NewRightHandSibling;
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // Copy the old index entry to the new one
        RtlCopyMemory(NewIndexEntry, (*MedianKey)->Entry, (*MedianKey)->Entry->EntryLength);

        // Use the new index entry after freeing the old one
        ExFreePoolWithTag((*MedianKey)->Entry, TAG_NTFS);
        (*MedianKey)->Entry = NewIndexEntry;

        // Update the length for the VCN
        (*MedianKey)->Entry->EntryLength += sizeof(ULONGLONG);

        // Set the node flag
        (*MedianKey)->Entry->Flags |= INDEX_ENTRY_NODE;
    }

    // "Node" will become the child of the median key
    (*MedianKey)->ChildNode = Node;
    SetIndexEntryVCN((*MedianKey)->Entry, Node->VCN);

    // Update Node's KeyCount (remember to add 1 for the new dummy key)
    // Node->KeyCount = MedianKeyIndex + 2;

    // KeyCount = CountBTreeKeys(Node->FirstKey);
    // ASSERT(Node->KeyCount == KeyCount);

    // everything to the right of MedianKey becomes the right hand sibling of Node
    (*NewRightHandSibling)->FirstKey = FirstKeyAfterMedian;
    // (*NewRightHandSibling)->KeyCount = CountBTreeKeys(FirstKeyAfterMedian);

#ifndef NDEBUG
    // DPRINT1("Left-hand node after split:\n");
    // DumpBTreeNode(Tree, Node, 0, 0);

    // DPRINT1("Right-hand sibling node after split:\n");
    // DumpBTreeNode(Tree, *NewRightHandSibling, 0, 0);
#endif

    return STATUS_SUCCESS;
}

NTSTATUS
Directory::NtfsInsertKey(ULONGLONG FileReference,
                         PFileNameEx FileNameAttribute,
                         PBTreeNode Node,
                         BOOLEAN CaseSensitive,
                         ULONG MaxIndexRootSize,
                         ULONG IndexRecordSize,
                         PBTreeKey *MedianKey,
                         PBTreeNode *NewRightHandSibling)
{
    PBTreeKey NewKey, CurrentKey, PreviousKey;
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG NodeSize;
    ULONG AllocatedNodeSize;
    ULONG MaxNodeSizeWithoutHeader;

    *MedianKey = NULL;
    *NewRightHandSibling = NULL;

    DPRINT("NtfsInsertKey(0x%I64x, %p, %p, %s, %lu, %lu, %p, %p)\n",
           FileReference,
           FileNameAttribute,
           Node,
           CaseSensitive ? "TRUE" : "FALSE",
           MaxIndexRootSize,
           IndexRecordSize,
           MedianKey,
           NewRightHandSibling);

    // Create the key for the filename attribute
    NewKey = CreateBTreeKeyFromFilename(FileReference, FileNameAttribute);
    if (!NewKey)
        return STATUS_INSUFFICIENT_RESOURCES;

    // Find where to insert the key
    CurrentKey = Node->FirstKey;
    PreviousKey = NULL;
    while (CurrentKey)
    {
        // Should the New Key go before the current key?
        LONG Comparison = CompareTreeKeys(NewKey, CurrentKey, CaseSensitive);

        if (Comparison == 0)
        {
            DPRINT1("\t\tComparison == 0: %.*S\n", GetFileName(NewKey)->NameLength, GetFileName(NewKey)->Name);
            DPRINT1("\t\tComparison == 0: %.*S\n", GetFileName(CurrentKey)->NameLength, GetFileName(CurrentKey)->Name);
        }
        ASSERT(Comparison != 0);

        // Is NewKey < CurrentKey?
        if (Comparison < 0)
        {
            // Does CurrentKey have a sub-node?
            if (CurrentKey->ChildNode)
            {
                PBTreeKey NewLeftKey;
                PBTreeNode NewChild;

                // Insert the key into the child node
                Status = NtfsInsertKey(FileReference,
                                       FileNameAttribute,
                                       CurrentKey->ChildNode,
                                       CaseSensitive,
                                       MaxIndexRootSize,
                                       IndexRecordSize,
                                       &NewLeftKey,
                                       &NewChild);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("ERROR: Failed to insert key.\n");
                    delete NewKey;
                    return Status;
                }

                // Did the child node get split?
                if (NewLeftKey)
                {
                    ASSERT(NewChild != NULL);

                    // Insert the new left key to the left of the current key
                    NewLeftKey->NextKey = CurrentKey;

                    // Is CurrentKey the first key?
                    if (!PreviousKey)
                        Node->FirstKey = NewLeftKey;
                    else
                        PreviousKey->NextKey = NewLeftKey;

                    // CurrentKey->LesserChild will be the right-hand sibling
                    CurrentKey->ChildNode = NewChild;

                    // Node->KeyCount++;
                    // Node->DiskNeedsUpdating = TRUE;

#ifndef NDEBUG
                    DumpFileTree();
#endif
                }
            }
            else
            {
                // Insert New Key before Current Key
                NewKey->NextKey = CurrentKey;

                // Increase KeyCount and mark node as dirty
                // Node->KeyCount++;
                // Node->DiskNeedsUpdating = TRUE;

                // was CurrentKey the first key?
                if (CurrentKey == Node->FirstKey)
                    Node->FirstKey = NewKey;
                else
                    PreviousKey->NextKey = NewKey;
            }
            break;
        }

        PreviousKey = CurrentKey;
        CurrentKey = CurrentKey->NextKey;
    }

    // Determine how much space the index entries will need
    NodeSize = GetSizeOfIndexEntries(Node);

    // Is Node not the root node?
    if (Node != RootNode)
    {
        // Calculate maximum size of index entries without any headers
        AllocatedNodeSize = IndexRecordSize - FIELD_OFFSET(IndexBuffer, IndexHeader);

        // TODO: Replace magic with math
        MaxNodeSizeWithoutHeader = AllocatedNodeSize - 0x28;

        // Has the node grown larger than its allocated size?
        if (NodeSize > MaxNodeSizeWithoutHeader)
        {
            Status = SplitBTreeNode(Node, MedianKey, NewRightHandSibling, CaseSensitive);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("ERROR: Failed to split B-Tree node!\n");
                return Status;
            }

            return Status;
        }
    }

    // NewEntry and NewKey will be destroyed later by DestroyBTree()

    return Status;
}

#define INDEX_ROOT_SMALL 0x0
#define INDEX_ROOT_LARGE 0x1

NTSTATUS
Directory::CreateIndexRootFromBTree(ULONG MaxIndexSize,
                                    PIndexRootEx *IndexRoot,
                                    ULONG *Length)
{
    PBTreeKey CurrentKey;
    PIndexEntry CurrentNodeEntry;
    PIndexRootEx NewIndexRoot = (PIndexRootEx)
                                ExAllocatePoolWithTag(NonPagedPool,
                                                      Volume->MFT->FileRecordSize,
                                                      TAG_NTFS);

    DPRINT("CreateIndexRootFromBTree(0x%lx, %p, %p)\n", MaxIndexSize, IndexRoot, Length);

#ifndef NDEBUG
    DumpFileTree();
#endif

    if (!NewIndexRoot)
    {
        DPRINT1("Failed to allocate memory for Index Root!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Setup the new index root
    RtlZeroMemory(NewIndexRoot, Volume->MFT->FileRecordSize);

    NewIndexRoot->AttributeType = TypeFileName;
    NewIndexRoot->CollationRule = ATTRDEF_COLLATION_FILENAME;
    NewIndexRoot->BytesPerIndexRec = (Volume->ClustersPerIndexRecord
                                      * Volume->SectorsPerCluster
                                      * Volume->BytesPerSector);
    // If Bytes per index record is less than cluster size, clusters per index record becomes sectors per index
    if (NewIndexRoot->BytesPerIndexRec < (Volume->BytesPerSector * Volume->SectorsPerCluster))
        NewIndexRoot->ClusPerIndexRec = NewIndexRoot->BytesPerIndexRec / Volume->BytesPerSector;
    else
        NewIndexRoot->ClusPerIndexRec = NewIndexRoot->BytesPerIndexRec
                                        / (Volume->BytesPerSector * Volume->SectorsPerCluster);

    // Setup the Index node header
    NewIndexRoot->Header.IndexOffset = sizeof(IndexNodeHeader);
    NewIndexRoot->Header.Flags = INDEX_ROOT_SMALL;

    // Start summing the total size of this node's entries
    NewIndexRoot->Header.TotalIndexSize = NewIndexRoot->Header.IndexOffset;

    // Setup each Node Entry
    CurrentKey = RootNode->FirstKey;
    CurrentNodeEntry = (PIndexEntry)((ULONG_PTR)NewIndexRoot
                                                + FIELD_OFFSET(IndexRootEx, Header)
                                                + NewIndexRoot->Header.IndexOffset);
    while (CurrentKey)
    {
        // Would adding the current entry to the index increase the index size beyond the limit we've set?
        ULONG IndexSize = NewIndexRoot->Header.TotalIndexSize - NewIndexRoot->Header.IndexOffset + CurrentKey->Entry->EntryLength;
        if (IndexSize > MaxIndexSize)
        {
            DPRINT1("TODO: Adding file would require creating an attribute list!\n");
            delete NewIndexRoot;
            return STATUS_NOT_IMPLEMENTED;
        }

        ASSERT(CurrentKey->Entry->EntryLength != 0);

        // Copy the index entry
        RtlCopyMemory(CurrentNodeEntry, CurrentKey->Entry, CurrentKey->Entry->EntryLength);

        DPRINT1("Index Node Entry Stream Length: %u\nIndex Node Entry Length: %u\n",
                CurrentNodeEntry->StreamLength,
                CurrentNodeEntry->EntryLength);

        // Does the current key have any sub-nodes?
        if (CurrentKey->ChildNode)
            NewIndexRoot->Header.Flags = INDEX_ROOT_LARGE;

        // Add Length of Current Entry to Total Size of Entries
        NewIndexRoot->Header.TotalIndexSize += CurrentKey->Entry->EntryLength;

        // Go to the next node entry
        CurrentNodeEntry = (PIndexEntry)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->EntryLength);

        CurrentKey = CurrentKey->NextKey;
    }

    // This is copied from the old driver, but seems to be very wrong.
    NewIndexRoot->Header.AllocatedSize = NewIndexRoot->Header.TotalIndexSize;

    *IndexRoot = NewIndexRoot;
    *Length = NewIndexRoot->Header.AllocatedSize + FIELD_OFFSET(IndexRootEx, Header);

    return STATUS_SUCCESS;
}

#if 0
NTSTATUS
Directory::UpdateIndexNode(PDEVICE_EXTENSION DeviceExt,
                           PFILE_RECORD_HEADER FileRecord,
                           PB_TREE_FILENAME_NODE Node,
                           ULONG IndexBufferSize,
                           PNTFS_ATTR_CONTEXT IndexAllocationContext,
                           ULONG IndexAllocationOffset)
{
    ULONG i;
    PBTreeKey CurrentKey = Node->FirstKey;
    BOOLEAN HasChildren = FALSE;
    NTSTATUS Status;


    DPRINT("UpdateIndexNode(%p, %p, %p, %lu, %p, %lu) called for index node with VCN %I64u\n",
           DeviceExt,
           FileRecord,
           Node,
           IndexBufferSize,
           IndexAllocationContext,
           IndexAllocationOffset,
           Node->VCN);

    // Walk through the node and look for children to update
    while (CurrentKey)
    {
        // If there's a child node
        if (CurrentKey->ChildNode)
        {
            HasChildren = TRUE;

            // Update the child node on disk
            Status = UpdateIndexNode(DeviceExt, FileRecord, CurrentKey->ChildNode, IndexBufferSize, IndexAllocationContext, IndexAllocationOffset);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("ERROR: Failed to update child node!\n");
                return Status;
            }

            // Is the Index Entry large enough to store the VCN?
            if (!BooleanFlagOn(CurrentKey->Entry->Flags, INDEX_ENTRY_NODE))
            {
                // Allocate memory for the larger index entry
                PIndexEntry NewEntry = (PIndexEntry)ExAllocatePoolWithTag(NonPagedPool,
                                                                          CurrentKey->Entry->EntryLength + sizeof(ULONGLONG),
                                                                          TAG_NTFS);
                if (!NewEntry)
                {
                    DPRINT1("ERROR: Unable to allocate memory for new index entry!\n");
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                // Copy the old entry to the new one
                RtlCopyMemory(NewEntry, CurrentKey->Entry, CurrentKey->Entry->EntryLength);

                NewEntry->EntryLength += sizeof(ULONGLONG);

                // Free the old memory
                delete CurrentKey->Entry;

                CurrentKey->Entry = NewEntry;
            }

            // Update the VCN stored in the index entry of CurrentKey
            SetIndexEntryVCN(CurrentKey->Entry, CurrentKey->ChildNode->VCN);

            CurrentKey->Entry->Flags |= INDEX_ENTRY_NODE;
        }

        CurrentKey = CurrentKey->NextKey;
    }


    // Do we need to write this node to disk?
    if (Node->DiskNeedsUpdating)
    {
        ULONGLONG NodeOffset;
        ULONG LengthWritten;
        PIndexBuffer IndexBuffer;

        // Does the node need to be assigned a VCN?
        if (!Node->HasValidVCN)
        {
            // Allocate the node
            Status = AllocateIndexNode(DeviceExt,
                                       FileRecord,
                                       IndexBufferSize,
                                       IndexAllocationContext,
                                       IndexAllocationOffset,
                                       &Node->VCN);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("ERROR: Failed to allocate index record in index allocation!\n");
                return Status;
            }

            Node->HasValidVCN = TRUE;
        }

        // Allocate memory for an index buffer
        IndexBuffer = (PIndexBuffer)ExAllocatePoolWithTag(NonPagedPool, IndexBufferSize, TAG_NTFS);
        if (!IndexBuffer)
        {
            DPRINT1("ERROR: Failed to allocate %lu bytes for index buffer!\n", IndexBufferSize);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // Create the index buffer we'll be writing to disk to represent this node
        Status = CreateIndexBufferFromBTreeNode(DeviceExt, Node, IndexBufferSize, HasChildren, IndexBuffer);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("ERROR: Failed to create index buffer from node!\n");
            ExFreePoolWithTag(IndexBuffer, TAG_NTFS);
            return Status;
        }

        // Get Offset of index buffer in index allocation
        NodeOffset = GetAllocationOffsetFromVCN(Node->VCN);

        // Write the buffer to the index allocation
        Status = WriteAttribute(DeviceExt, IndexAllocationContext, NodeOffset, (const PUCHAR)IndexBuffer, IndexBufferSize, &LengthWritten, FileRecord);
        if (!NT_SUCCESS(Status) || LengthWritten != IndexBufferSize)
        {
            DPRINT1("ERROR: Failed to update index allocation!\n");
            ExFreePoolWithTag(IndexBuffer, TAG_NTFS);
            if (!NT_SUCCESS(Status))
                return Status;
            else
                return STATUS_END_OF_FILE;
        }

        Node->DiskNeedsUpdating = FALSE;

        // Free the index buffer
        ExFreePoolWithTag(IndexBuffer, TAG_NTFS);
    }

    return STATUS_SUCCESS;
}
#endif