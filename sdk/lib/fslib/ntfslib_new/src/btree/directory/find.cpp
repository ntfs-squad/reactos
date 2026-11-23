/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

static LONG
CompareName(PUNICODE_STRING FileName,
            PFileNameEx IndexFileName)
{
    const USHORT Length = min(FileName->Length, IndexFileName->NameLength * sizeof(WCHAR));
    UNICODE_STRING IndexName;

    RtlInitEmptyUnicodeString(&IndexName, IndexFileName->Name, Length);
    FileName->Length = Length;
    IndexName.Length = Length;

    return RtlCompareUnicodeString(FileName, &IndexName, TRUE);
}

PBTreeKey
Directory::FindKeyInNode(PUNICODE_STRING FileName,
                         PBTreeKey Key)
{
    for (PBTreeKey CurrentKey = Key; CurrentKey; CurrentKey = CurrentKey->NextKey)
    {
        if (DoesFileNameMatch(FileName, CurrentKey))
            return CurrentKey;

        if (CurrentKey->ChildNode &&
            (CurrentKey->IsLastEntry() || CompareName(FileName, GetFileName(CurrentKey)) <= 0))
        {
            return FindKeyInNode(FileName, CurrentKey->ChildNode->FirstKey);
        }
    }

    return NULL;
}

NTSTATUS
Directory::FindFile(_Inout_ PUNICODE_STRING FileName,
                    _Out_   PULONGLONG FileRecordNumber)
{
    PBTreeKey FoundKey = FindKeyInNode(FileName, RootNode.FirstKey);
    if (!FoundKey)
        return STATUS_NOT_FOUND;

    *FileRecordNumber = GetFRNFromFileRef(FileRef(FoundKey));
    return STATUS_SUCCESS;
}
