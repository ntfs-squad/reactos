/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Log File Service (LFS) Implementation
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"

#define RESTART_PAGE_2_OFFSET 4096

LogFileService::LogFileService(_In_ PNTFSVolume TargetVolume)
{
    // Store volume pointer
    Volume = TargetVolume;
}

LogFileService::~LogFileService()
{
    delete LogFileData;
}

NTSTATUS
LogFileService::InitializeLFS()
{
    NTSTATUS Status;
    PAttribute FileDataAttr;
    ULONG LogFileSize;

    // Find the Log File.
    Status = Volume->MFT->GetFileRecord(_LogFile, &LogFile);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to find $LogFile!\n");
        return Status;
    }

    // Initialize the logfile data buffer
    FileDataAttr = LogFile->GetAttribute(TypeData, NULL);
    if (!FileDataAttr)
    {
        DPRINT1("Failed to find $DATA attribute!\n");
        delete LogFile;
        return STATUS_NOT_FOUND;
    }

    LogFileSize = GetAttributeDataSize(FileDataAttr);
    LogFileData = new(PagedPool) UCHAR[LogFileSize];

    // Copy the data from the log file
    Status = LogFile->CopyData(FileDataAttr,
                               LogFileData,
                               &LogFileSize);

    // Set the restart page pointers.
    RestartPage1 = (PLfsRestartPage)LogFileData;
    RestartPage2 = (PLfsRestartPage)(LogFileData + RESTART_PAGE_2_OFFSET);

    // TODO: Pick a best restart page based on corruption and recency.
    ClientMajorVersion = RestartPage1->MajorVersion;
    ClientMinorVersion = RestartPage1->MinorVersion;

    if (!IsSupportedClientVersion())
    {
        DPRINT1("Client version not supported! (%ld.%ld)\n", ClientMajorVersion, ClientMinorVersion);
        RestartPage1 = NULL;
        RestartPage2 = NULL;
        delete LogFileData;
        Status = STATUS_LOG_BLOCK_VERSION;
        goto Done;
    }

    // Perform file system recovery.
    Status = PerformFileSystemRecovery();

    if (!NT_SUCCESS(Status))
        DPRINT1("Failed to perform self-healing!\n");

Done:
    delete LogFile;
    return Status;
}

NTSTATUS
LogFileService::PerformFileSystemRecovery()
{
    /* Scan $LogFile and the file system for issues.
     *     - If gBugCheckOnCorrupt is FALSE (default):
     *         Fix found issues, if possible.
     *     - If gBugCheckOnCorrupt is TRUE:
     *         Bugcheck on found issues.
     */

    DPRINT1("PerformFileSystemRecovery() is a STUB!\n");
    return STATUS_SUCCESS;
}

NTSTATUS
LogFileService::ShutdownLFS()
{
    /* Perform any cleanup necessary before shutting down LFS.
     * MS NTFS does at least these things:
     *     - Ensure RestartPage1 and RestartPage2 are identical.
     *     - Convert client version back from 2.0 to 1.1
     */

    DPRINT1("ShutdownLFS() is a STUB!\n");
    return STATUS_SUCCESS;
}