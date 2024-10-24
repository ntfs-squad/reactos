/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
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

PBTreeKey
FindKeyInNode(PBTreeKey Key,
              PWCHAR FileName,
              UINT Length)
{
    PBTreeKey CurrentKey;

    // Start the search with the first key
    CurrentKey = Key;

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
            DPRINT1("Searching node...\n");
            return FindKeyInNode(Key->ChildNode->FirstKey, FileName, Length);
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

NTSTATUS
Directory::FindNextFile(_In_  PWCHAR FileName,
                        _In_  ULONG Length,
                        _Out_ PULONGLONG FileRecordNumber)
{
    PBTreeKey FoundKey;

    // For now, start scan at beginning.
    FoundKey = FindKeyInNode(RootNode->FirstKey, FileName, Length);

    if (!FoundKey)
    {
        DPRINT1("Failed to find file!\n");
        return STATUS_NOT_FOUND;
    }

    CurrentKey = FoundKey;
    *FileRecordNumber = GetFRNFromFileRef(FoundKey->Entry->Data.Directory.IndexedFile);
    return STATUS_SUCCESS;
}