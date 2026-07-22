/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

static NTSTATUS
DestroyBTreeNode(PBTreeNode Node);

static NTSTATUS
DestroyBTreeKey(PBTreeKey Key)
{
    if (Key->Entry)
        delete Key->Entry;

    if (Key->ChildNode)
        DestroyBTreeNode(Key->ChildNode);

    delete Key;
    return STATUS_SUCCESS;
}

static NTSTATUS
DestroyBTreeNode(PBTreeNode Node)
{
    PBTreeKey NextKey = NULL;
    PBTreeKey CurrentKey = Node->FirstKey;

    while (CurrentKey)
    {
        NextKey = CurrentKey->NextKey;
        DestroyBTreeKey(CurrentKey);
        CurrentKey = NextKey;
    }

    ASSERT(NextKey == NULL);
    delete Node;
    return STATUS_SUCCESS;
}

BTree::~BTree()
{
    // DestroyBTreeNode() also handles a root without keys.
    if (RootNode)
        DestroyBTreeNode(RootNode);
}

NTSTATUS
BTree::ResetCurrentKey()
{
    CurrentKey = RootNode->FirstKey;
    return STATUS_SUCCESS;
}