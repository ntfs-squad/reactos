/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2017 Trevor Thompson
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#include "../io/ntfsprocs.h"

#define NDEBUG

// Complete
PBTreeKey
CreateBTreeKeyFromFilename(ULONGLONG FileReference, PFileNameEx FileNameAttribute)
{
    PBTreeKey NewKey;
    ULONG AttributeSize = GetFileNameAttributeLength(FileNameAttribute);
    ULONG EntrySize = ALIGN_UP_BY(AttributeSize + FIELD_OFFSET(IndexEntry, FileName), 8);

    DPRINT1("CreateBTreeKeyFromFilename() called!\n");
    PrintFilenameAttrHeader(FileNameAttribute);

    // Create a new Index Entry for the file
    PIndexEntry NewEntry = (PIndexEntry)ExAllocatePoolWithTag(NonPagedPool,
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
    NewEntry->EntryLength = EntrySize;
    NewEntry->StreamLength = AttributeSize;

    // Copy the FileNameAttribute
    RtlCopyMemory(&(NewEntry->FileName),
                  FileNameAttribute,
                  AttributeSize);

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
        ULONG IndexSize = NewIndexRoot->Header.TotalIndexSize -
                          NewIndexRoot->Header.IndexOffset +
                          CurrentKey->IndexEntry->EntryLength;

        if (IndexSize > MaxIndexSize)
        {
            DPRINT1("TODO: Adding file would require creating an attribute list!\n");
            delete NewIndexRoot;
            return STATUS_NOT_IMPLEMENTED;
        }

        ASSERT(CurrentKey->IndexEntry->Length != 0);

        // Copy the index entry
        RtlCopyMemory(CurrentNodeEntry,
                      CurrentKey->IndexEntry,
                      CurrentKey->IndexEntry->EntryLength);

        DPRINT1("Index Node Entry Stream Length: %u\nIndex Node Entry Length: %u\n",
                CurrentNodeEntry->KeyLength,
                CurrentNodeEntry->Length);

        // Does the current key have any sub-nodes?
        if (CurrentKey->LesserChild)
            NewIndexRoot->Header.Flags = INDEX_ROOT_LARGE;

        // Add Length of Current Entry to Total Size of Entries
        NewIndexRoot->Header.TotalIndexSize += CurrentKey->IndexEntry->EntryLength;

        // Go to the next node entry
        CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->Length);

        CurrentKey = CurrentKey->NextKey;
    }

    NewIndexRoot->Header.AllocatedSize = NewIndexRoot->Header.TotalIndexSize;

    *IndexRoot = NewIndexRoot;
    *Length = NewIndexRoot->Header.AllocatedSize + FIELD_OFFSET(IndexRootEx, Header);

    return STATUS_SUCCESS;
}

/**
* @name AllocateIndexNode
* @implemented
*
* Allocates a new index record in an index allocation.
*
* @param Volume
* Pointer to the target PNTFSVolume describing the volume the node will be created on.
*
* @param FileRecord
* Pointer to a copy of the file record containing the index.
*
* @param IndexBufferSize
* Size of an index record for this index, in bytes. Commonly defined as 4096.
*
* @param IndexAllocationCtx
* Pointer to an NTFS_ATTR_CONTEXT describing the index allocation attribute the node will be assigned to.
*
* @param IndexAllocationOffset
* Offset of the index allocation attribute relative to the file record.
*
* @param NewVCN
* Pointer to a ULONGLONG which will receive the VCN of the newly-assigned index record
*
* @returns
* STATUS_SUCCESS in case of success.
* STATUS_NOT_IMPLEMENTED if there's no $I30 bitmap attribute in the file record.
*
* @remarks
* AllocateIndexNode() doesn't write any data to the index record it creates. Called by UpdateIndexNode().
* Don't call PrintAllVCNs() or NtfsDumpFileRecord() after calling AllocateIndexNode() before UpdateIndexNode() finishes.
* Possible TODO: Create an empty node and write it to the allocated index node, so the index allocation is always valid.
*/
NTSTATUS
AllocateIndexNode(PNTFSVolume Volume,
                  PFileRecord FileRecord,
                  ULONG IndexBufferSize,
                  PAttribute IndexAllocationAttribute,
                  ULONG IndexAllocationOffset,
                  PULONGLONG NewVCN)
{
    NTSTATUS Status;
    PAttribute BitmapAttribute;
    ULONGLONG IndexAllocationLength, BitmapLength;
    // ULONG BitmapOffset;
    ULONGLONG NextNodeNumber;
    PCHAR *BitmapMem;
    ULONG *BitmapPtr;
    RTL_BITMAP Bitmap;
    ULONG BytesWritten;
    ULONG BytesNeeded;
    ULONGLONG DataSize;

    DPRINT1("AllocateIndexNode(%p, %p, %lu, %lu, %p) called.\n",
            Volume,
            FileRecord,
            IndexBufferSize,
            IndexAllocationOffset,
            NewVCN);

    // Get the length of the attribute allocation
    IndexAllocationLength = AttributeDataLength(IndexAllocationAttribute);

    // Find the bitmap attribute for the index
    Status = FileRecord->GetAttribute(AttributeType::Bitmap, L"$I30", BitmapAttribute);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("FIXME: Need to add bitmap attribute!\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    // Get the length of the bitmap attribute
    BitmapLength = AttributeDataLength(BitmapAttribute);
    NextNodeNumber = IndexAllocationLength / BytesPerIndexRecord(Volume);

    // TODO: Find unused allocation in bitmap and use that space first

    // See how many bytes we need to store the amount of bits we'll have
    BytesNeeded = NextNodeNumber / 8;

    // Add another bit to bitmap
    BytesNeeded++;

    // Windows seems to allocate the bitmap in 8-byte chunks to keep any bytes from being wasted on padding
    BytesNeeded = ALIGN_UP(BytesNeeded, ATTR_RECORD_ALIGNMENT);

    // Allocate memory for the bitmap, including some padding; RtlInitializeBitmap() wants a pointer
    // that's ULONG-aligned, and it wants the size of the memory allocated for it to be a ULONG-multiple.
    BitmapMem = (PCHAR*)ExAllocatePoolWithTag(NonPagedPool, BytesNeeded + sizeof(ULONG), TAG_NTFS);
    if (!BitmapMem)
    {
        DPRINT1("Error: failed to allocate bitmap!");
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    // RtlInitializeBitmap() wants a pointer that's ULONG-aligned.
    BitmapPtr = (PULONG)ALIGN_UP_BY((ULONG_PTR)BitmapMem, sizeof(ULONG));
    RtlZeroMemory(BitmapPtr, BytesNeeded);

    // Read the existing bitmap data
    Status = FileRecord->CopyData(BitmapAttribute, (PUCHAR)BitmapPtr, &BitmapLength);

    // Initialize bitmap
    RtlInitializeBitMap(&Bitmap, BitmapPtr, NextNodeNumber);

    // Do we need to enlarge the bitmap?
    if (BytesNeeded > AttributeDataLength(BitmapAttribute))
    {
        // TODO: handle synchronization issues that could occur from changing the directory's file record
        // Change bitmap size
        DataSize = BytesNeeded;
        Status = FileRecord->SetDataLength(BitmapAttribute, &DataSize);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("ERROR: Failed to set length of bitmap attribute!\n");
            goto cleanup;
        }
    }

    // Enlarge Index Allocation attribute
    DataSize = IndexAllocationLength + IndexBufferSize;
    Status = FileRecord->SetDataLength(IndexAllocationAttribute, &DataSize);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("ERROR: Failed to set length of index allocation!\n");
        goto cleanup;
    }

#if 0
    // Update file record on disk
    Status = UpdateFileRecord(Volume, IndexAllocationCtx->FileMFTIndex, FileRecord);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("ERROR: Failed to update file record!\n");
        goto cleanup;
    }

    // Set the bit for the new index record
    RtlSetBits(&Bitmap, NextNodeNumber, 1);

    // Write the new bitmap attribute
    Status = WriteAttribute(Volume,
                            BitmapCtx,
                            0,
                            (const PUCHAR)BitmapPtr,
                            BytesNeeded,
                            &BytesWritten,
                            FileRecord);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("ERROR: Unable to write to $I30 bitmap attribute!\n");
    }

    // Calculate VCN of new node number
    *NewVCN = NextNodeNumber * (IndexBufferSize / BytesPerCluster(Volume));
    DPRINT("New VCN: %I64u\n", *NewVCN);
#endif

cleanup:
    if (BitmapMem)
        delete BitmapMem;
    if (BitmapAttribute)
        delete BitmapAttribute;

    return Status;
}

NTSTATUS
UpdateIndexNode(PNTFSVolume Volume,
                PFILE_RECORD_HEADER FileRecord,
                PBTreeFilenameNode Node,
                ULONG IndexBufferSize,
                PNTFS_ATTR_CONTEXT IndexAllocationContext,
                ULONG IndexAllocationOffset)
{
    ULONG i;
    PBTreeKey CurrentKey = Node->FirstKey;
    BOOLEAN HasChildren = FALSE;
    NTSTATUS Status;


    DPRINT("UpdateIndexNode(%p, %p, %p, %lu, %p, %lu) called for index node with VCN %I64u\n",
           Volume,
           FileRecord,
           Node,
           IndexBufferSize,
           IndexAllocationContext,
           IndexAllocationOffset,
           Node->VCN);

    // Walk through the node and look for children to update
    for (i = 0; i < Node->KeyCount; i++)
    {
        ASSERT(CurrentKey);

        // If there's a child node
        if (CurrentKey->LesserChild)
        {
            HasChildren = TRUE;

            // Update the child node on disk
            Status = UpdateIndexNode(Volume, FileRecord, CurrentKey->LesserChild, IndexBufferSize, IndexAllocationContext, IndexAllocationOffset);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("ERROR: Failed to update child node!\n");
                return Status;
            }

            // Is the Index Entry large enough to store the VCN?
            if (!BooleanFlagOn(CurrentKey->IndexEntry->Flags, NTFS_INDEX_ENTRY_NODE))
            {
                // Allocate memory for the larger index entry
                PINDEX_ENTRY_ATTRIBUTE NewEntry = (PINDEX_ENTRY_ATTRIBUTE)ExAllocatePoolWithTag(
                                                                          NonPagedPool,
                                                                          CurrentKey->IndexEntry->Length +
                                                                          sizeof(ULONGLONG),
                                                                          TAG_NTFS);
                if (!NewEntry)
                {
                    DPRINT1("ERROR: Unable to allocate memory for new index entry!\n");
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                // Copy the old entry to the new one
                RtlCopyMemory(NewEntry, CurrentKey->IndexEntry, CurrentKey->IndexEntry->Length);

                NewEntry->Length += sizeof(ULONGLONG);

                // Free the old memory
                delete CurrentKey->IndexEntry;

                CurrentKey->IndexEntry = NewEntry;
            }

            // Update the VCN stored in the index entry of CurrentKey
            SetIndexEntryVCN(CurrentKey->IndexEntry, CurrentKey->LesserChild->VCN);

            CurrentKey->IndexEntry->Flags |= NTFS_INDEX_ENTRY_NODE;
        }

        CurrentKey = CurrentKey->NextKey;
    }


    // Do we need to write this node to disk?
    if (Node->DiskNeedsUpdating)
    {
        ULONGLONG NodeOffset;
        ULONG LengthWritten;
        PINDEX_BUFFER IndexBuffer;

        // Does the node need to be assigned a VCN?
        if (!Node->HasValidVCN)
        {
            // Allocate the node
            Status = AllocateIndexNode(Volume,
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
        IndexBuffer = (PINDEX_BUFFER)ExAllocatePoolWithTag(NonPagedPool, IndexBufferSize, TAG_NTFS);
        if (!IndexBuffer)
        {
            DPRINT1("ERROR: Failed to allocate %lu bytes for index buffer!\n", IndexBufferSize);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // Create the index buffer we'll be writing to disk to represent this node
        Status = CreateIndexBufferFromBTreeNode(Volume, Node, IndexBufferSize, HasChildren, IndexBuffer);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("ERROR: Failed to create index buffer from node!\n");
            delete IndexBuffer;
            return Status;
        }

        // Get Offset of index buffer in index allocation
        NodeOffset = GetAllocationOffsetFromVCN(Volume, IndexBufferSize, Node->VCN);

        // Write the buffer to the index allocation
        Status = WriteAttribute(Volume, IndexAllocationContext, NodeOffset, (const PUCHAR)IndexBuffer, IndexBufferSize, &LengthWritten, FileRecord);
        if (!NT_SUCCESS(Status) || LengthWritten != IndexBufferSize)
        {
            DPRINT1("ERROR: Failed to update index allocation!\n");
            delete IndexBuffer;
            if (!NT_SUCCESS(Status))
                return Status;
            else
                return STATUS_END_OF_FILE;
        }

        Node->DiskNeedsUpdating = FALSE;

        // Free the index buffer
        delete IndexBuffer;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
UpdateIndexAllocation(PNTFSVolume Volume,
                      PBTree Tree,
                      ULONG IndexBufferSize,
                      PFILE_RECORD_HEADER FileRecord)
{
    // Find the index allocation and bitmap
    PNTFS_ATTR_CONTEXT IndexAllocationContext;
    PBTreeKey CurrentKey;
    NTSTATUS Status;
    BOOLEAN HasIndexAllocation = FALSE;
    ULONG i;
    ULONG IndexAllocationOffset;

    DPRINT("UpdateIndexAllocation() called.\n");

    Status = FindAttribute(Volume, FileRecord, AttributeIndexAllocation, L"$I30", 4, &IndexAllocationContext, &IndexAllocationOffset);
    if (NT_SUCCESS(Status))
    {
        HasIndexAllocation = TRUE;

#ifdef NTFS_DEBUG
        PrintAllVCNs(Volume,
                     IndexAllocationContext,
                     IndexBufferSize);
#endif
    }
    // Walk through the root node and update all the sub-nodes
    CurrentKey = Tree->RootNode->FirstKey;
    for (i = 0; i < Tree->RootNode->KeyCount; i++)
    {
        if (CurrentKey->LesserChild)
        {
            if (!HasIndexAllocation)
            {
                // We need to add an index allocation to the file record
                PNTFS_ATTR_RECORD EndMarker = (PNTFS_ATTR_RECORD)((ULONG_PTR)FileRecord + FileRecord->BytesInUse - (sizeof(ULONG) * 2));
                DPRINT1("Adding index allocation...\n");

                // Add index allocation to the very end of the file record
                Status = AddIndexAllocation(Volume,
                                            FileRecord,
                                            EndMarker,
                                            L"$I30",
                                            4);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("ERROR: Failed to add index allocation!\n");
                    return Status;
                }

                // Find the new attribute
                Status = FindAttribute(Volume, FileRecord, AttributeIndexAllocation, L"$I30", 4, &IndexAllocationContext, &IndexAllocationOffset);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("ERROR: Couldn't find newly-created index allocation!\n");
                    return Status;
                }

                // Advance end marker
                EndMarker = (PNTFS_ATTR_RECORD)((ULONG_PTR)EndMarker + EndMarker->Length);

                // Add index bitmap to the very end of the file record
                Status = AddBitmap(Volume,
                                   FileRecord,
                                   EndMarker,
                                   L"$I30",
                                   4);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("ERROR: Failed to add index bitmap!\n");
                    ReleaseAttributeContext(IndexAllocationContext);
                    return Status;
                }

                HasIndexAllocation = TRUE;
            }

            // Is the Index Entry large enough to store the VCN?
            if (!BooleanFlagOn(CurrentKey->IndexEntry->Flags, NTFS_INDEX_ENTRY_NODE))
            {
                // Allocate memory for the larger index entry
                PINDEX_ENTRY_ATTRIBUTE NewEntry = (PINDEX_ENTRY_ATTRIBUTE)ExAllocatePoolWithTag(
                                                                          NonPagedPool,
                                                                          CurrentKey->IndexEntry->Length +
                                                                          sizeof(ULONGLONG),
                                                                          TAG_NTFS);
                if (!NewEntry)
                {
                    DPRINT1("ERROR: Unable to allocate memory for new index entry!\n");
                    if (HasIndexAllocation)
                        ReleaseAttributeContext(IndexAllocationContext);
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                // Copy the old entry to the new one
                RtlCopyMemory(NewEntry, CurrentKey->IndexEntry, CurrentKey->IndexEntry->Length);

                NewEntry->Length += sizeof(ULONGLONG);

                // Free the old memory
                delete CurrentKey->IndexEntry;

                CurrentKey->IndexEntry = NewEntry;
                CurrentKey->IndexEntry->Flags |= NTFS_INDEX_ENTRY_NODE;
            }

            // Update the sub-node
            Status = UpdateIndexNode(Volume, FileRecord, CurrentKey->LesserChild, IndexBufferSize, IndexAllocationContext, IndexAllocationOffset);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("ERROR: Failed to update index node!\n");
                ReleaseAttributeContext(IndexAllocationContext);
                return Status;
            }

            // Update the VCN stored in the index entry of CurrentKey
            SetIndexEntryVCN(CurrentKey->IndexEntry, CurrentKey->LesserChild->VCN);
        }
        CurrentKey = CurrentKey->NextKey;
    }

#ifdef NTFS_DEBUG
    DumpBTree(Tree);
#endif

    if (HasIndexAllocation)
    {
#ifdef NTFS_DEBUG
        PrintAllVCNs(Volume,
                     IndexAllocationContext,
                     IndexBufferSize);
#endif
        ReleaseAttributeContext(IndexAllocationContext);
    }

    return STATUS_SUCCESS;
}

/**
* @name NtfsInsertKey
* @implemented
*
* Inserts a FileNameEx into a B-Tree node.
*
* @param Tree
* Pointer to the BTree the key (filename attribute) is being inserted into.
*
* @param FileReference
* Reference number to the file being added. This will be a combination of the MFT index and update sequence number.
*
* @param FileNameAttribute
* Pointer to a FileNameEx which is the data for the key that will be added to the tree. A copy will be made.
*
* @param Node
* Pointer to a BTreeFilenameNode into which a new key will be inserted, in order.
*
* @param CaseSensitive
* Boolean indicating if the function should operate in case-sensitive mode. This will be TRUE
* if an application created the file with the FILE_FLAG_POSIX_SEMANTICS flag.
*
* @param MaxIndexRootSize
* The maximum size, in bytes, of node entries that can be stored in the index root before it will grow too large for
* the file record. This number is just the size of the entries, without any headers for the attribute or index root.
*
* @param IndexRecordSize
* The size, in bytes, of an index record for this index. AKA an index buffer. Usually set to 4096.
*
* @param MedianKey
* Pointer to a PBTreeKey that will receive a pointer to the median key, should the node grow too large and need to be split.
* Will be set to NULL if the node isn't split.
*
* @param NewRightHandSibling
* Pointer to a PBTreeFilenameNode that will receive a pointer to a newly-created right-hand sibling node,
* should the node grow too large and need to be split. Will be set to NULL if the node isn't split.
*
* @remarks
* A node is always sorted, with the least comparable filename stored first and a dummy key to mark the end.
*/
NTSTATUS
NtfsInsertKey(PBTree Tree,
              ULONGLONG FileReference,
              PFileNameEx FileNameAttribute,
              PBTreeFilenameNode Node,
              BOOLEAN CaseSensitive,
              ULONG MaxIndexRootSize,
              ULONG IndexRecordSize,
              PBTreeKey *MedianKey,
              PBTreeFilenameNode *NewRightHandSibling)
{
    PBTreeKey NewKey, CurrentKey, PreviousKey;
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG NodeSize;
    ULONG AllocatedNodeSize;
    ULONG MaxNodeSizeWithoutHeader;
    ULONG i;

    *MedianKey = NULL;
    *NewRightHandSibling = NULL;

    DPRINT("NtfsInsertKey(%p, 0x%I64x, %p, %p, %s, %lu, %lu, %p, %p)\n",
           Tree,
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
    for (i = 0; i < Node->KeyCount; i++)
    {
        // Should the New Key go before the current key?
        LONG Comparison = CompareTreeKeys(NewKey, CurrentKey, CaseSensitive);

        if (Comparison == 0)
        {
            DPRINT1("\t\tComparison == 0: %.*S\n", NewKey->IndexEntry->FileName.NameLength, NewKey->IndexEntry->FileName.Filename);
            DPRINT1("\t\tComparison == 0: %.*S\n", CurrentKey->IndexEntry->FileName.NameLength, CurrentKey->IndexEntry->FileName.Filename);
        }
        ASSERT(Comparison != 0);

        // Is NewKey < CurrentKey?
        if (Comparison < 0)
        {
            // Does CurrentKey have a sub-node?
            if (CurrentKey->LesserChild)
            {
                PBTreeKey NewLeftKey;
                PBTreeFilenameNode NewChild;

                // Insert the key into the child node
                Status = NtfsInsertKey(Tree,
                                       FileReference,
                                       FileNameAttribute,
                                       CurrentKey->LesserChild,
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
                    CurrentKey->LesserChild = NewChild;

                    Node->KeyCount++;
                    Node->DiskNeedsUpdating = TRUE;

#ifdef NTFS_DEBUG
                    DumpBTree(Tree);
#endif
                }
            }
            else
            {
                // Insert New Key before Current Key
                NewKey->NextKey = CurrentKey;

                // Increase KeyCount and mark node as dirty
                Node->KeyCount++;
                Node->DiskNeedsUpdating = TRUE;

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
    if (Node != Tree->RootNode)
    {
        // Calculate maximum size of index entries without any headers
        AllocatedNodeSize = IndexRecordSize - FIELD_OFFSET(INDEX_BUFFER, Header);

        // TODO: Replace magic with math
        MaxNodeSizeWithoutHeader = AllocatedNodeSize - 0x28;

        // Has the node grown larger than its allocated size?
        if (NodeSize > MaxNodeSizeWithoutHeader)
        {
            NTSTATUS Status;

            Status = SplitBTreeNode(Tree, Node, MedianKey, NewRightHandSibling, CaseSensitive);
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

/**
* @name SplitBTreeNode
* @implemented
*
* Splits a B-Tree node that has grown too large. Finds the median key and sets up a right-hand-sibling
* node to contain the keys to the right of the median key.
*
* @param Tree
* Pointer to the BTree which contains the node being split
*
* @param Node
* Pointer to the BTreeFilenameNode that needs to be split
*
* @param MedianKey
* Pointer a PBTreeKey that will receive the pointer to the key in the middle of the node being split
*
* @param NewRightHandSibling
* Pointer to a PBTreeFilenameNode that will receive a pointer to a newly-created BTreeFilenameNode
* containing the keys to the right of MedianKey.
*
* @param CaseSensitive
* Boolean indicating if the function should operate in case-sensitive mode. This will be TRUE
* if an application created the file with the FILE_FLAG_POSIX_SEMANTICS flag.
*
* @return
* STATUS_SUCCESS on success.
* STATUS_INSUFFICIENT_RESOURCES if an allocation fails.
*
* @remarks
* It's the responsibility of the caller to insert the new median key into the parent node, as well as making the
* NewRightHandSibling the lesser child of the node that is currently Node's parent.
*/
NTSTATUS
SplitBTreeNode(PBTree Tree,
               PBTreeFilenameNode Node,
               PBTreeKey *MedianKey,
               PBTreeFilenameNode *NewRightHandSibling,
               BOOLEAN CaseSensitive)
{
    ULONG MedianKeyIndex;
    PBTreeKey LastKeyBeforeMedian, FirstKeyAfterMedian;
    ULONG KeyCount;
    ULONG HalfSize;
    ULONG SizeSum;
    ULONG i;

    DPRINT("SplitBTreeNode(%p, %p, %p, %p, %s) called\n",
            Tree,
            Node,
            MedianKey,
            NewRightHandSibling,
            CaseSensitive ? "TRUE" : "FALSE");

#ifdef NTFS_DEBUG
    DumpBTreeNode(Tree, Node, 0, 0);
#endif

    // Create the right hand sibling
    *NewRightHandSibling = new(NonPagedPool) BTreeFilenameNode();
    if (*NewRightHandSibling == NULL)
    {
        DPRINT1("Error: Failed to allocate memory for right hand sibling!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(*NewRightHandSibling, sizeof(BTreeFilenameNode));
    (*NewRightHandSibling)->DiskNeedsUpdating = TRUE;


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
    for (i = 0; i < Node->KeyCount; i++)
    {
        SizeSum += LastKeyBeforeMedian->IndexEntry->Length;

        if (SizeSum > HalfSize)
            break;

        MedianKeyIndex++;
        LastKeyBeforeMedian = LastKeyBeforeMedian->NextKey;
    }

    // Now we can get the median key and the key that follows it
    *MedianKey = LastKeyBeforeMedian->NextKey;
    FirstKeyAfterMedian = (*MedianKey)->NextKey;

    DPRINT1("%lu keys, %lu median\n", Node->KeyCount, MedianKeyIndex);
    DPRINT1("\t\tMedian: %.*S\n", (*MedianKey)->IndexEntry->FileName.NameLength, (*MedianKey)->IndexEntry->FileName.Filename);

    // "Node" will be the left hand sibling after the split, containing all keys prior to the median key

    // We need to create a dummy pointer at the end of the LHS. The dummy's child will be the median's child.
    LastKeyBeforeMedian->NextKey = CreateDummyKey(BooleanFlagOn((*MedianKey)->IndexEntry->Flags, NTFS_INDEX_ENTRY_NODE));
    if (LastKeyBeforeMedian->NextKey == NULL)
    {
        DPRINT1("Error: Couldn't allocate dummy key!\n");
        LastKeyBeforeMedian->NextKey = *MedianKey;
        delete *NewRightHandSibling;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Did the median key have a child node?
    if ((*MedianKey)->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
    {
        // Set the child of the new dummy key
        LastKeyBeforeMedian->NextKey->LesserChild = (*MedianKey)->LesserChild;

        // Give the dummy key's index entry the same sub-node VCN the median
        SetIndexEntryVCN(LastKeyBeforeMedian->NextKey->IndexEntry, GetIndexEntryVCN((*MedianKey)->IndexEntry));
    }
    else
    {
        // Median key didn't have a child node, but it will. Create a new index entry large enough to store a VCN.
        PINDEX_ENTRY_ATTRIBUTE NewIndexEntry = (PINDEX_ENTRY_ATTRIBUTE)ExAllocatePoolWithTag(
                                                                       NonPagedPool,
                                                                       (*MedianKey)->IndexEntry->Length +
                                                                       sizeof(ULONGLONG),
                                                                       TAG_NTFS);
        if (!NewIndexEntry)
        {
            DPRINT1("Unable to allocate memory for new index entry!\n");
            LastKeyBeforeMedian->NextKey = *MedianKey;
            delete *NewRightHandSibling;
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // Copy the old index entry to the new one
        RtlCopyMemory(NewIndexEntry, (*MedianKey)->IndexEntry, (*MedianKey)->IndexEntry->Length);

        // Use the new index entry after freeing the old one
        delete (*MedianKey)->IndexEntry;
        (*MedianKey)->IndexEntry = NewIndexEntry;

        // Update the length for the VCN
        (*MedianKey)->IndexEntry->Length += sizeof(ULONGLONG);

        // Set the node flag
        (*MedianKey)->IndexEntry->Flags |= NTFS_INDEX_ENTRY_NODE;
    }

    // "Node" will become the child of the median key
    (*MedianKey)->LesserChild = Node;
    SetIndexEntryVCN((*MedianKey)->IndexEntry, Node->VCN);

    // Update Node's KeyCount (remember to add 1 for the new dummy key)
    Node->KeyCount = MedianKeyIndex + 2;

    KeyCount = CountBTreeKeys(Node->FirstKey);
    ASSERT(Node->KeyCount == KeyCount);

    // everything to the right of MedianKey becomes the right hand sibling of Node
    (*NewRightHandSibling)->FirstKey = FirstKeyAfterMedian;
    (*NewRightHandSibling)->KeyCount = CountBTreeKeys(FirstKeyAfterMedian);

#ifdef NTFS_DEBUG
    DPRINT1("Left-hand node after split:\n");
    DumpBTreeNode(Tree, Node, 0, 0);

    DPRINT1("Right-hand sibling node after split:\n");
    DumpBTreeNode(Tree, *NewRightHandSibling, 0, 0);
#endif

    return STATUS_SUCCESS;
}

/**
* @name DemoteBTreeRoot
* @implemented
*
* Demoting the root means first putting all the keys in the root node into a new node, and making
* the new node a child of a dummy key. The dummy key then becomes the sole contents of the root node.
* The B-Tree gets one level deeper. This operation is needed when an index root grows too large for its file record.
* Demotion is my own term; I might change the name later if I think of something more descriptive or can find
* an appropriate name for this operation in existing B-Tree literature.
*
* @param Tree
* Pointer to the BTree whose root is being demoted
*
* @returns
* STATUS_SUCCESS on success.
* STATUS_INSUFFICIENT_RESOURCES if an allocation fails.
*/
NTSTATUS
DemoteBTreeRoot(PBTree Tree)
{
    PBTreeFilenameNode NewSubNode, NewIndexRoot;
    PBTreeKey DummyKey;

    DPRINT("Collapsing Index Root into sub-node.\n");

#ifdef NTFS_DEBUG
    DumpBTree(Tree);
#endif

    // Create a new node that will hold the keys currently in index root
    NewSubNode = new(NonPagedPool) BTreeFilenameNode();
    if (!NewSubNode)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new sub-node.\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(NewSubNode, sizeof(BTreeFilenameNode));

    // Copy the applicable data from the old index root node
    NewSubNode->KeyCount = Tree->RootNode->KeyCount;
    NewSubNode->FirstKey = Tree->RootNode->FirstKey;
    NewSubNode->DiskNeedsUpdating = TRUE;

    // Create a new dummy key, and make the new node it's child
    DummyKey = CreateDummyKey(TRUE);
    if (!DummyKey)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new root node.\n");
        delete NewSubNode;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Make the new node a child of the dummy key
    DummyKey->LesserChild = NewSubNode;

    // Create a new index root node
    NewIndexRoot = new(NonPagedPool) BTreeFilenameNode();
    if (!NewIndexRoot)
    {
        DPRINT1("ERROR: Couldn't allocate memory for new index root.\n");
        delete NewSubNode;
        delete DummyKey;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(NewIndexRoot, sizeof(BTreeFilenameNode));

    NewIndexRoot->DiskNeedsUpdating = TRUE;

    // Insert the dummy key into the new node
    NewIndexRoot->FirstKey = DummyKey;
    NewIndexRoot->KeyCount = 1;
    NewIndexRoot->DiskNeedsUpdating = TRUE;

    // Make the new node the Tree's root node
    Tree->RootNode = NewIndexRoot;

#ifdef NTFS_DEBUG
    DumpBTree(Tree);
#endif

    return STATUS_SUCCESS;
}

NTSTATUS
CreateIndexBufferFromBTreeNode(PNTFSVolume Volume,
                               PBTreeFilenameNode Node,
                               ULONG BufferSize,
                               BOOLEAN HasChildren,
                               PINDEX_BUFFER IndexBuffer)
{
    ULONG i;
    PBTreeKey CurrentKey;
    PINDEX_ENTRY_ATTRIBUTE CurrentNodeEntry;
    NTSTATUS Status;

    // TODO: Fix magic, do math
    RtlZeroMemory(IndexBuffer, BufferSize);
    RtlCopyMemory(&(IndexBuffer->RecordHeader.TypeID),
                  "INDX",
                  4);
    IndexBuffer->RecordHeader.UpdateSequenceOffset = 0x28;
    IndexBuffer->RecordHeader.SizeOfUpdateSequence = 9;

    // TODO: Check bitmap for VCN
    ASSERT(Node->HasValidVCN);
    IndexBuffer->VCN = Node->VCN;

    // Windows seems to alternate between using 0x28 and 0x40 for the first entry offset of each index buffer.
    // Interestingly, neither Windows nor chkdsk seem to mind if we just use 0x28 for every index record.
    IndexBuffer->IndexHeader.IndexOffset = 0x28;
    IndexBuffer->IndexHeader.AllocatedSize = BufferSize - FIELD_OFFSET(INDEX_BUFFER, IndexHeader);

    // Start summing the total size of this node's entries
    IndexBuffer->IndexHeader.TotalIndexSize = IndexBuffer->IndexHeader.IndexOffset;

    CurrentKey = Node->FirstKey;
    CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)&(IndexBuffer->IndexHeader)
                                                + IndexBuffer->IndexHeader.IndexOffset);
    for (i = 0; i < Node->KeyCount; i++)
    {
        // Would adding the current entry to the index increase the node size beyond the allocation size?
        ULONG IndexSize = FIELD_OFFSET(INDEX_BUFFER, IndexHeader)
            + IndexBuffer->IndexHeader.TotalIndexSize
            + CurrentNodeEntry->Length;
        if (IndexSize > BufferSize)
        {
            DPRINT1("TODO: Adding file would require creating a new node!\n");
            return STATUS_NOT_IMPLEMENTED;
        }

        ASSERT(CurrentKey->IndexEntry->Length != 0);

        // Copy the index entry
        RtlCopyMemory(CurrentNodeEntry, CurrentKey->IndexEntry, CurrentKey->IndexEntry->Length);

        DPRINT("Index Node Entry Stream Length: %u\nIndex Node Entry Length: %u\n",
               CurrentNodeEntry->KeyLength,
               CurrentNodeEntry->Length);

        // Add Length of Current Entry to Total Size of Entries
        IndexBuffer->IndexHeader.TotalIndexSize += CurrentNodeEntry->Length;

        // Check for child nodes
        if (HasChildren)
            IndexBuffer->IndexHeader.Flags = INDEX_NODE_LARGE;

        // Go to the next node entry
        CurrentNodeEntry = (PINDEX_ENTRY_ATTRIBUTE)((ULONG_PTR)CurrentNodeEntry + CurrentNodeEntry->Length);
        CurrentKey = CurrentKey->NextKey;
    }

    Status = AddFixupArray(Volume, &IndexBuffer->RecordHeader);

    return Status;
}

/**
* @name SetIndexEntryVCN
* @implemented
*
* Sets the VCN of a given IndexEntry.
*
* @param IndexEntry
* Pointer to an INDEX_ENTRY_ATTRIBUTE structure that will have its VCN set.
*
* @param VCN
* VCN to store in the index entry.
*
* @remarks
* The index entry must have enough memory allocated to store the VCN, and must have the NTFS_INDEX_ENTRY_NODE flag set.
* The VCN of an index entry is stored at the very end of the structure, after the filename attribute. Since the filename
* attribute can be a variable size, this function makes setting this member easy.
*/
VOID
SetIndexEntryVCN(PINDEX_ENTRY_ATTRIBUTE IndexEntry, ULONGLONG VCN)
{
    PULONGLONG Destination = (PULONGLONG)((ULONG_PTR)IndexEntry + IndexEntry->Length - sizeof(ULONGLONG));
    ASSERT(IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE);
    *Destination = VCN;
}
