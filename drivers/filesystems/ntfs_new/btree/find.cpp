/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

/* INCLUDES *****************************************************************/
#include "../io/ntfsprocs.h"

// NOTE: This is case sensitive!
INT
WideStringCompare(PWCHAR FirstString,
                  PWCHAR SecondString,
                  UINT Length)
{
    INT Result;

    for(int i = 0; i < Length; i++)
    {
        Result = (FirstString[i] - SecondString[i]);
        if (Result)
            return Result;
    }

    return 0;
}

// TODO: Actually leverage btree for fast searching instead of searching linearly

PBTreeKey
FindKeyInNode(PBTreeNode Node,
              PWCHAR FileName,
              UINT Length)
{
    PBTreeKey CurrentKey, ResumeKey;

    // Start the search with the first key
    CurrentKey = Node->FirstKey;

    DPRINT1("FindKeyInNode() called!\n");

    // Strip * and \ characters from end of string
    if (wcschr(FileName, L'*') || wcschr(FileName, L'\\'))
    {
        Length = min((wcschr(FileName, L'*') - FileName),
                     (wcschr(FileName, L'\\') - FileName));
    }

    while(CurrentKey)
    {
        if (RtlCompareMemory(GetFileName(CurrentKey)->Name,
                             FileName,
                             Length) == Length)
        {
            // We found the key!
            return CurrentKey;
        }

        // We can skip this node if we're greater than the filename of the node
        if (CurrentKey->Entry->Flags & INDEX_ENTRY_NODE &&
            (WideStringCompare(FileName, GetFileName(CurrentKey)->Name, Length) <= 0 ||
            CurrentKey->Entry->Flags & INDEX_ENTRY_END))
        {
            // If it's not in this node, it's not in here.
            ResumeKey = CurrentKey;
            DPRINT1("Searching node...\n");
            return FindKeyInNode(CurrentKey->ChildNode, FileName, Length);
        }

        if (CurrentKey->Entry->Flags & INDEX_ENTRY_END)
        {
            // We've reached the end of this node and checked if it was an index node.
            return NULL;
        }

        // Go to the next key
        CurrentKey = CurrentKey->NextKey;
    }

    // We didn't find the key
    return NULL;
}