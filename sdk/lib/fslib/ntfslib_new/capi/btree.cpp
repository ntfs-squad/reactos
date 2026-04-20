/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for BTree class and related classes
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"

extern "C" NTSTATUS
NTAPI
NtfsDirectoryGetFileBothDirInfo(
    _In_    PNtfsDirectory Dir,
    _In_    BOOLEAN ReturnSingleEntry,
    _In_    BOOLEAN RestartScan,
    _In_    PUNICODE_STRING FileNameFilter,
    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
    _Inout_ PULONG BufferLength)
{
    return reinterpret_cast<PDirectory>(Dir)->GetFileBothDirInfo(ReturnSingleEntry,
                                                                 RestartScan,
                                                                 FileNameFilter,
                                                                 Buffer,
                                                                 BufferLength);
}