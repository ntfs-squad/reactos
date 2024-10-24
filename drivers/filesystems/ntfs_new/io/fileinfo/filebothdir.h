/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */
#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

static
NTSTATUS
GetFileBothDirectoryInformation(_In_    PFileContextBlock FileCB,
                                _In_    PVolumeContextBlock VolCB,
                                _In_    UCHAR IrpFlags,
                                _In_    PUNICODE_STRING FileName,
                                _Out_   PFILE_BOTH_DIR_INFORMATION Buffer,
                                _Inout_ PULONG Length)
{
    NTSTATUS Status;
    Directory* FileDir;
    BOOLEAN ReturnSingleEntry, RestartScan;

    ASSERT(FileCB);
    FileDir = FileCB->FileDir;
    ReturnSingleEntry = !!(IrpFlags & SL_RETURN_SINGLE_ENTRY);
    RestartScan = !!(IrpFlags & SL_RESTART_SCAN);

    if (!FileDir)
    {
        DPRINT1("This is not a directory!\n");
        return STATUS_NOT_FOUND;
    }

    Status = FileDir->GetFileBothDirInfo(ReturnSingleEntry,
                                         RestartScan,
                                         Buffer,
                                         Length);

    return Status;
}