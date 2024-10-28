/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#define GetFileName(Key) \
((PFileNameEx)((Key)->Entry->IndexStream))

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
                          PBTreeKey Key)
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

    /* Note: if we want to do case-insensitive searching, we must make
     * NameFilter all uppercase.
     *
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtlisnameinexpression
     */

    return FsRtlIsNameInExpression(NameFilter,
                                   &FileNameString,
                                   FALSE,
                                   NULL);
}