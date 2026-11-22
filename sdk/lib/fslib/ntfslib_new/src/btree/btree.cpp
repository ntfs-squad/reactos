/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

BTreeKey::~BTreeKey()
{
    delete ChildNode;
    if (Entry)
        NtfsFreePool(Entry);
}

BTreeNode::~BTreeNode()
{
    for (BTreeKey* Key = FirstKey; Key;)
    {
        BTreeKey* Next = Key->NextKey;
        delete Key;
        Key = Next;
    }
}

BTreeKey*
BTreeKey::NextInTree() const
{
    if (ChildNode)
        return ChildNode->FirstKey;
    if (IsLastEntry())
        return ParentNodeKey ? ParentNodeKey->NextKey : NULL;
    return NextKey;
}
