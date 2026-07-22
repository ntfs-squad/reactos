/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define PathElementLength(FileName) \
(wcschr(FileName, L'\\')) \
? (wcschr(FileName, L'\\') - FileName) \
: (wcschr(FileName, L':')) \
? (wcschr(FileName, L':') - FileName) \
: (wcslen(FileName))

#define CompareLength(FileName, CurrentKey) \
min(PathElementLength(FileName->Buffer), GetFileName(CurrentKey)->NameLength)

static LONG
WideStringCompare(PWCHAR FirstString,
                  PWCHAR SecondString,
                  ULONG Length)
{
    UNICODE_STRING String1, String2;

    RtlInitEmptyUnicodeString(&String1, FirstString, Length);
    RtlInitEmptyUnicodeString(&String2, SecondString, Length);
    String1.Length = Length;
    String2.Length = Length;

    return RtlCompareUnicodeString(&String1, &String2, TRUE);
}

PBTreeKey
Directory::FindKeyInNode(PUNICODE_STRING FileName,
                         PBTreeKey Key)
{
    PBTreeKey CurrentKey;

    // Start the search with the first key
    CurrentKey = Key;

    while(CurrentKey)
    {
        if (DoesFileNameMatch(FileName, CurrentKey))
        {
            // We found the key!
            return CurrentKey;
        }

        // We can skip this node if we're greater than the filename of the node
        if (CurrentKey->Entry->Flags & INDEX_ENTRY_NODE &&
            (WideStringCompare(FileName->Buffer,
                               GetFileName(CurrentKey)->Name,
                               CompareLength(FileName, CurrentKey)) <= 0 ||
            CurrentKey->Entry->Flags & INDEX_ENTRY_END))
        {
            // If it's not in this node, it's not in here.
            return FindKeyInNode(FileName, CurrentKey->ChildNode->FirstKey);
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
                        _Out_ PULONGLONG FileRecordNumber)
{
    UNICODE_STRING FileNameString;
    PBTreeKey FoundKey;

    RtlInitEmptyUnicodeString(&FileNameString, FileName, (USHORT)(wcslen(FileName) * sizeof(WCHAR)));
    FileNameString.Length = (USHORT)((PathElementLength(FileName)) * sizeof(WCHAR));

    /* Upcase the search name once for the case-insensitive matching done
     * in DoesFileNameMatch() (see the note there).
     */
    RtlUpcaseUnicodeString(&FileNameString, &FileNameString, FALSE);

    // For now, start scan at beginning.
    FoundKey = FindKeyInNode(&FileNameString, RootNode->FirstKey);

    if (!FoundKey)
        return STATUS_NOT_FOUND;

    CurrentKey = FoundKey;
    *FileRecordNumber = GetFRNFromFileRef(FileRef(FoundKey));
    return STATUS_SUCCESS;
}