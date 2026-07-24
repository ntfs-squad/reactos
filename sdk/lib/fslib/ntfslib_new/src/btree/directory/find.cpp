/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

static ULONG
PathElementLength(_In_ PCWSTR FileName)
{
    PCWSTR Separator = NtfsWcsChr(FileName, L'\\');

    if (!Separator)
        Separator = NtfsWcsChr(FileName, L':');

    return Separator ? (ULONG)(Separator - FileName) : NtfsWcsLen(FileName);
}

static LONG
CompareFileName(_In_ PUNICODE_STRING FileName,
                _In_ PFileNameEx IndexedName)
{
    UNICODE_STRING IndexedNameString;

    IndexedNameString = NtfsMakeCountedUnicodeString(
        IndexedName->Name,
        IndexedName->NameLength * sizeof(WCHAR));

    return RtlCompareUnicodeString(FileName, &IndexedNameString, TRUE);
}

PBTreeKey
Directory::FindKeyInNode(PUNICODE_STRING FileName,
                         PBTreeKey Key)
{
    PBTreeKey CurrentKey;
    LONG CompareResult;

    // Start the search with the first key
    CurrentKey = Key;

    while(CurrentKey)
    {
        if (DoesFileNameMatch(FileName, CurrentKey))
        {
            // We found the key!
            return CurrentKey;
        }

        CompareResult = CurrentKey->Entry->Flags & INDEX_ENTRY_END
                        ? -1
                        : CompareFileName(FileName, GetFileName(CurrentKey));

        /* Index entries are sorted. Descend through the first entry not less
         * than the target; if it has no child, the target cannot occur later.
         */
        if (CurrentKey->Entry->Flags & INDEX_ENTRY_NODE &&
            (CompareResult <= 0 ||
             CurrentKey->Entry->Flags & INDEX_ENTRY_END))
        {
            return FindKeyInNode(FileName, CurrentKey->ChildNode->FirstKey);
        }

        if (CompareResult < 0)
            return NULL;

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
    NTSTATUS Status;
    ULONG ElementLength;
    UNICODE_STRING FileNameString;
    PBTreeKey FoundKey;

    ElementLength = PathElementLength(FileName);
    if (ElementLength > MAXUSHORT / sizeof(WCHAR))
        return STATUS_NAME_TOO_LONG;

    FileNameString = NtfsMakeCountedUnicodeString(
        FileName,
        (USHORT)(ElementLength * sizeof(WCHAR)));

    Status = DiskVolume->UpcaseWideString(
        FileNameString.Buffer,
        FileNameString.Length / sizeof(WCHAR));
    if (!NT_SUCCESS(Status))
        return Status;

    // For now, start scan at beginning.
    FoundKey = FindKeyInNode(&FileNameString, RootNode->FirstKey);

    if (!FoundKey)
        return STATUS_NOT_FOUND;

    CurrentKey = FoundKey;
    *FileRecordNumber = GetFRNFromFileRef(FileRef(FoundKey));
    return STATUS_SUCCESS;
}
