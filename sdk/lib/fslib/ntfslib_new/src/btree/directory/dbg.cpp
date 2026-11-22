/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

static
void
DumpBTreeNode(PBTreeNode Node,
              UINT Depth);
static
void
DumpBTreeKey(PBTreeKey Key,
             ULONG Depth)
{
    for (ULONG i = 0; i < Depth; i++)
        DbgPrint("    ");

    if (!Key->IsLastEntry())
    {
        UNICODE_STRING FileName;
        FileName.Length = GetFileName(Key)->NameLength * sizeof(WCHAR);
        FileName.MaximumLength = FileName.Length;
        FileName.Buffer = GetFileName(Key)->Name;
        DbgPrint("|===%wZ (MFT ID: %ld, Flags: 0x%X)\n",
                 &FileName,
                 GetFRNFromFileRef(FileRef(Key)),
                 Key->Flags);
    }
    else
    {
        DbgPrint("L===(Dummy Key)\n");
    }

    // Is there a child node?
    if (Key->ChildNode)
        DumpBTreeNode(Key->ChildNode, (Depth + 1));
}

static
void
DumpBTreeNode(PBTreeNode Node,
              UINT Depth)
{
    PBTreeKey CurrentKey;
    CurrentKey = Node->FirstKey;
    while(CurrentKey)
    {
        DumpBTreeKey(CurrentKey, Depth);
        CurrentKey = CurrentKey->NextKey;
    }
}

void
Directory::DumpFileTree()
{
    // Print header
    DbgPrint("Index Root\n");
    DumpBTreeNode(&RootNode, 0);
}
