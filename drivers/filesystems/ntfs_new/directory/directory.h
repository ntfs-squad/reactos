/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#define GetFileName(Key) \
((PFileNameEx)((Key)->Entry->IndexStream))

#define IsLastEntry(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_END)

#define IsIndexNode(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_NODE)

#define IsEndOfNode(Key) IsLastEntry(Key) && !IsIndexNode(Key)

#define GetNextKey(Key) \
IsIndexNode(Key) ? (Key)->ChildNode->FirstKey : IsEndOfNode(Key) ? \
(Key)->ParentNodeKey ? (Key)->ParentNodeKey->NextKey : NULL : (Key)->NextKey

#define DIRECTORY_BTREE_DUPLICATE_SHORTNAME 1

class Directory : BTree
{
public:
    // ./directory.cpp
    // ~Directory();
    NTSTATUS
    LoadDirectory(_In_ PFileRecord File);

    // ./find.cpp
    NTSTATUS
    FindNextFile(_In_  PWCHAR FileName,
                 _Out_ PULONGLONG FileRecordNumber);

    // ./get.cpp
    NTSTATUS
    GetFileBothDirInfo(_In_    BOOLEAN ReturnSingleEntry,
                       _In_    BOOLEAN RestartScan,
                       _In_    PUNICODE_STRING FileNameFilter,
                       _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                       _Inout_ PULONG BufferLength);

    // ./dbg.cpp
    void
    DumpFileTree();
};

static
inline
BOOLEAN DoesFileNameMatch(PUNICODE_STRING NameFilter,
                          PBTreeKey Key,
                          BOOLEAN IgnoreCase = TRUE)
{
    UNICODE_STRING FileNameString;
    PFileNameEx FileNameData;

    if (Key->Entry->Flags & INDEX_ENTRY_END)
    {
        // This is a dummy key, it will not match with any file.
        return FALSE;
    }

    FileNameData = GetFileName(Key);
    RtlInitEmptyUnicodeString(&FileNameString,
                              FileNameData->Name,
                              ((FileNameData->NameLength) * sizeof(WCHAR)));
    FileNameString.Length = FileNameString.MaximumLength;

    if (IgnoreCase)
    {
        /* Note: if we want to do case-insensitive searching, we must make
         * NameFilter all uppercase.
         *
         * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtlisnameinexpression
        */
        NTSTATUS Status = RtlUpcaseUnicodeString(NameFilter, NameFilter, FALSE);
        if (!NT_SUCCESS(Status))
        {
            // Failed to upcase name filter! Perform case sensitive matching...
            __debugbreak();
            IgnoreCase = FALSE;
        }
    }

    return FsRtlIsNameInExpression(NameFilter,
                                   &FileNameString,
                                   IgnoreCase,
                                   NULL);
}

#define IsLowercaseCharacterW(wchar) wchar >= L'a' && wchar <= L'z'

#define IsDotOrDotDotDirW(Buffer, Length) \
Buffer[0] == L'.' \
? Length == sizeof(WCHAR) || (Buffer[1] == L'.' && Length == 2 * sizeof(WCHAR)) \
: FALSE

static inline
BOOLEAN
IsLegalShortNameCharacterW(_In_ WCHAR Char)
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
static
BOOLEAN
IsLegal8Dot3ShortName(_In_ PWSTR Buffer,
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

static
PBTreeKey GetShortNameKey(_In_ PBTreeKey Key,
                          _In_ BOOLEAN SkipNonShortNames = TRUE)
{
    /* Search file tree for the next entry with the same file record
     * number (FRN).
     *
     * NOTE: I have seen short file name entries 3+ keys after
     * the long file name entry, but never before the long name entry
     * (see: system32). Most of the time, it's the next key.
     *
     * If you do observe a short file name entry before the long file
     * name entry, please update the search algorithm.
     */

    PBTreeKey FoundKey;
    PFileNameEx FileNameData;
    ULONGLONG TargetFRN;

    // If the key is already a legal short name, we don't need to search for another one.
    FileNameData = GetFileName(Key);
    if (IsLegal8Dot3ShortName(FileNameData->Name, FileNameData->NameLength))
        return NULL;

    TargetFRN = GetFRNFromFileRef(FileRef(Key));
    FoundKey = GetNextKey(Key);

    /* This algorithm does not go back to search from the beginning of the tree.
     * If you observe a short name bug, this is probably it.
     */
    while (FoundKey)
    {
        if (!SkipNonShortNames ||
            FoundKey->Flags & DIRECTORY_BTREE_DUPLICATE_SHORTNAME)
        {
            FileNameData = GetFileName(FoundKey);
            if (GetFRNFromFileRef(FileRef(FoundKey)) == TargetFRN
                && IsLegal8Dot3ShortName(FileNameData->Name, FileNameData->NameLength))
            {
                return FoundKey;
            }
        }
        FoundKey = GetNextKey(FoundKey);
    }

    /* If we don't find the short name, no short file name was generated.
     * This is not necessarily an error.
     */

    return NULL;
}