/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

BOOLEAN
Directory::IsLegalShortNameCharacterW(_In_ WCHAR Char)
{
    /* Legal characters include:
     * 0-9, A-Z, !, #, $, %, &, ', (, ), -, @, ^, _, `, {, }, ~
     */
    WCHAR AllowedCharacters[17] = L"!#$%&'()-@^_`{}~";

    if ((Char >= L'0' && Char <= L'9') || (Char >= L'A' && Char <= L'Z'))
        return TRUE;

    for (int i = 0; i < wcslen(AllowedCharacters); i++)
    {
        if (Char == AllowedCharacters[i])
            return TRUE;
    }

    return FALSE;
}

/* TODO:
 * Determine if this should be replaced with RtlIsNameLegalDOS8Dot3().
 *
 * RtlIsNameLegalDOS8Dot3() does not currently work for this purpose;
 * however our implementation may be incorrect. Write tests and experiment
 * on Windows.
 */
BOOLEAN
Directory::IsLegal8Dot3ShortName(_In_ PWSTR Buffer,
                                 _In_ USHORT Length)
{
    USHORT LengthBeforeDot, LengthAfterDot;
    BOOLEAN ReachedDot;

    // Fail if the length is too long.
    if (Length > MAX_SHORTNAME_LENGTH)
        return FALSE;

    // '.' and '..' are valid and special cases.
    if (IsDotOrDotDotDirW(Buffer, Length))
        return TRUE;

    ReachedDot = FALSE;
    LengthBeforeDot = 0;
    LengthAfterDot = 0;

    for (int i = 0; i < Length; i++)
    {
        // Handle dot character
        if (Buffer[i] == L'.')
        {
            if (ReachedDot)
            {
                return FALSE;
            }
            ReachedDot = TRUE;
            continue;
        }

        // Fail if this is an illegal character
        if (!(IsLegalShortNameCharacterW(Buffer[i]) ||
              IsLowercaseCharacterW(Buffer[i])))
        {
            return FALSE;
        }

        // Count this element
        if (!ReachedDot)
        {
            LengthBeforeDot++;
            if (LengthBeforeDot > 8)
            {
                return FALSE;
            }
        }

        else
        {
            LengthAfterDot++;
            if (LengthAfterDot > 3)
            {
                return FALSE;
            }
        }
    }

    // If a file name has an extension, it must at least be 1 character long.
    if (ReachedDot && !LengthAfterDot)
    {
        return FALSE;
    }

    // We passed all tests.
    return TRUE;

}

PBTreeKey
Directory::CreateDummyKey(BOOLEAN HasChildNode)
{
    PIndexEntry NewEntry;
    PBTreeKey NewKey;

    // Calculate max size of a dummy key
    ULONG EntrySize = ALIGN_UP_BY(FIELD_OFFSET(IndexEntry, Data), 8);
    EntrySize += sizeof(ULONGLONG); // for VCN

    // Create the index entry for the key
    NewEntry = (PIndexEntry)ExAllocatePoolWithTag(PagedPool, EntrySize, TAG_NTFS);
    if (!NewEntry)
    {
        DPRINT1("Couldn't allocate memory for dummy key index entry!\n");
        return NULL;
    }
    RtlZeroMemory(NewEntry, EntrySize);

    if (HasChildNode)
    {
        NewEntry->Flags = INDEX_ENTRY_NODE | INDEX_ENTRY_END;
    }

    else
    {
        NewEntry->Flags = INDEX_ENTRY_END;
        EntrySize -= sizeof(ULONGLONG); // no VCN
    }

    NewEntry->EntryLength = EntrySize;

    // Create the key
    NewKey = new(PagedPool) BTreeKey();
    if (!NewKey)
    {
        DPRINT1("Unable to allocate dummy key!\n");
        delete NewKey;
        return NULL;
    }

    RtlZeroMemory(NewKey, sizeof(BTreeKey));
    NewKey->Entry = NewEntry;
    return NewKey;
}

ULONG
Directory::GetSizeOfIndexEntries(PBTreeNode Node)
{
    // Start summing the total size of this node's entries
    ULONG NodeSize = 0;

    // Walk through the list of Node Entries
    PBTreeKey CurrentKey = Node->FirstKey;
    while (CurrentKey)
    {
        ASSERT(CurrentKey->Entry->EntryLength != 0);

        // Add the length of the current node
        NodeSize += CurrentKey->Entry->EntryLength;
        CurrentKey = CurrentKey->NextKey;
    }

    return NodeSize;
}

VOID
Directory::SetIndexEntryVCN(PIndexEntry IndexEntry, ULONGLONG VCN)
{
    PULONGLONG Destination = (PULONGLONG)((ULONG_PTR)IndexEntry + IndexEntry->EntryLength - sizeof(ULONGLONG));

    ASSERT(IndexEntry->Flags & INDEX_ENTRY_NODE);

    *Destination = VCN;
}

// LONG
// Directory::CompareTreeKeys(PBTreeKey Key1, PBTreeKey Key2, BOOLEAN CaseSensitive)
// {
//     UNICODE_STRING Key1Name, Key2Name;
//     LONG Comparison;

//     // Key1 must not be the final key (AKA the dummy key)
//     ASSERT(!(Key1->IndexEntry->Flags & INDEX_ENTRY_END));

//     // If Key2 is the "dummy key", key 1 will always come first
//     if (Key2->NextKey == NULL)
//         return -1;

//     Key1Name.Buffer = Key1->Entry->FileName.Name;
//     Key1Name.Length = Key1Name.MaximumLength = Key1->Entry->FileName.NameLength * sizeof(WCHAR);

//     Key2Name.Buffer = Key2->Entry->FileName.Name;
//     Key2Name.Length = Key2Name.MaximumLength = Key2->Entry->FileName.NameLength * sizeof(WCHAR);

//     // Are the two keys the same length?
//     if (Key1Name.Length == Key2Name.Length)
//         return RtlCompareUnicodeString(&Key1Name, &Key2Name, !CaseSensitive);

//     // Is Key1 shorter?
//     if (Key1Name.Length < Key2Name.Length)
//     {
//         // Truncate KeyName2 to be the same length as KeyName1
//         Key2Name.Length = Key1Name.Length;

//         // Compare the names of the same length
//         Comparison = RtlCompareUnicodeString(&Key1Name, &Key2Name, !CaseSensitive);

//         // If the truncated names are the same length, the shorter one comes first
//         if (Comparison == 0)
//             return -1;
//     }
//     else
//     {
//         // Key2 is shorter
//         // Truncate KeyName1 to be the same length as KeyName2
//         Key1Name.Length = Key2Name.Length;

//         // Compare the names of the same length
//         Comparison = RtlCompareUnicodeString(&Key1Name, &Key2Name, !CaseSensitive);

//         // If the truncated names are the same length, the shorter one comes first
//         if (Comparison == 0)
//             return 1;
//     }

//     return Comparison;
// }
