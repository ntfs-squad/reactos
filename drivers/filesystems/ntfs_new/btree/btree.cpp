/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

/* INCLUDES *****************************************************************/
#include "../io/ntfsprocs.h"

NTSTATUS
DestroyBTreeNode(PBTreeNode Node);

NTSTATUS
DestroyBTreeKey(PBTreeKey Key)
{
    if (Key->Entry)
        delete Key->Entry;

    if (Key->ChildNode)
        DestroyBTreeNode(Key->ChildNode);

    delete Key;
    return STATUS_SUCCESS;
}

NTSTATUS
DestroyBTreeNode(PBTreeNode Node)
{
    PBTreeKey NextKey;
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
    DestroyBTreeNode(RootNode);
    DPRINT1("BTree destroyed!\n");
}

NTSTATUS
BTree::ResetCurrentKey()
{
    CurrentKey = RootNode->FirstKey;
    return STATUS_SUCCESS;
}