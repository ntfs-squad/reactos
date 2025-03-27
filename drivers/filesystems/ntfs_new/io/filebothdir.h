/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

static
inline
BOOLEAN
ContainsWildcard(PUNICODE_STRING String)
{
    for (USHORT i = 0; i < String->Length; i++)
    {
        if (String->Buffer[i] == L'*'    ||
            String->Buffer[i] == L'?'    ||
            String->Buffer[i] == DOS_DOT ||
            String->Buffer[i] == DOS_QM  ||
            String->Buffer[i] == DOS_STAR)
            return TRUE;
    }
    return FALSE;
}

static
NTSTATUS
GetFileBothDirectoryInformation(_In_    PFileContextBlock FileCB,
                                _In_    UCHAR IrpFlags,
                                _In_    PUNICODE_STRING FileNameFilter,
                                _Out_   PFILE_BOTH_DIR_INFORMATION Buffer,
                                _Inout_ PULONG Length)
{
    Directory* FileDir;
    BOOLEAN ReturnSingleEntry, RestartScan;

    if (!FileCB)
    {
        DPRINT1("INVESTIGATE ME: GetFileBothDirectoryInformation() called with NULL FileCB!\n");
        return STATUS_INVALID_PARAMETER;
    }

    FileDir = FileCB->FileDir;
    RestartScan = !!(IrpFlags & SL_RESTART_SCAN);

    if (!FileDir)
    {
        DPRINT1("This is not a directory!\n");
        return STATUS_NOT_FOUND;
    }

    /* If there's no wild cards and a file name filter
     * is specified, we will only return one entry.
     */
    if (FileNameFilter &&
        !ContainsWildcard(FileNameFilter))
        ReturnSingleEntry = TRUE;
    else
        ReturnSingleEntry = !!(IrpFlags & SL_RETURN_SINGLE_ENTRY);

    return FileDir->GetFileBothDirInfo(ReturnSingleEntry,
                                       RestartScan,
                                       FileNameFilter,
                                       Buffer,
                                       Length);
}