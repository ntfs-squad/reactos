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
