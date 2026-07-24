/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Log File Service (LFS) Implementation
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#define INITIAL_SYSTEM_PAGE_SIZE 4096

static BOOLEAN
IsPowerOfTwo(_In_ ULONG Value)
{
    return Value != 0 && (Value & (Value - 1)) == 0;
}

static BOOLEAN
IsValidRestartPage(_In_ PLfsRestartPage Page,
                   _In_ ULONG PageSize)
{
    if (RtlCompareMemory(Page->Signature, "RSTR", 4) != 4 ||
        Page->SystemPageSize != PageSize ||
        Page->RestartOffset > PageSize ||
        sizeof(LfsRestartArea) > PageSize - Page->RestartOffset)
    {
        return FALSE;
    }

    return TRUE;
}

static UINT64
GetRestartPageLsn(_In_ PLfsRestartPage Page)
{
    return reinterpret_cast<PLfsRestartArea>(
        reinterpret_cast<PUCHAR>(Page) + Page->RestartOffset)->CurrentLsn;
}

LogFileService::LogFileService(_In_ PVolume TargetVolume)
{
    // Store volume pointer
    DiskVolume = TargetVolume;
    LogFile = NULL;
    LogFileData = NULL;
    RestartPage1 = NULL;
    RestartPage2 = NULL;
    ClientMajorVersion = 0;
    ClientMinorVersion = 0;
}

LogFileService::~LogFileService()
{
    delete[] LogFileData;
}

NTSTATUS
LogFileService::InitializeLFS()
{
    NTSTATUS Status;
    PAttribute FileDataAttr;
    ULONG BytesRemaining, LogFileDataSize, LogFileSize, SystemPageSize;
    BOOLEAN RestartPage1Valid, RestartPage2Valid;
    PLfsRestartPage SelectedPage;

    // Find the Log File.
    Status = DiskVolume->MFT->GetFileRecord(_LogFile, &LogFile);

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
    LogFileDataSize = min(LogFileSize, 2 * INITIAL_SYSTEM_PAGE_SIZE);
    if (LogFileDataSize < sizeof(LfsRestartPage))
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    LogFileData = new(PagedPool) UCHAR[LogFileDataSize];
    if (!LogFileData)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    // Read enough to discover the volume's system-page size.
    BytesRemaining = LogFileDataSize;
    Status = LogFile->CopyData(FileDataAttr,
                               LogFileData,
                               &BytesRemaining);
    if (!NT_SUCCESS(Status) || BytesRemaining != 0)
        goto Done;

    RestartPage1 = reinterpret_cast<PLfsRestartPage>(LogFileData);
    SystemPageSize = RestartPage1->SystemPageSize;
    if (!IsPowerOfTwo(SystemPageSize) ||
        SystemPageSize < DiskVolume->BytesPerSector ||
        SystemPageSize > 65536 ||
        LogFileSize < 2 * SystemPageSize)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    if (LogFileDataSize != 2 * SystemPageSize)
    {
        delete[] LogFileData;
        LogFileDataSize = 2 * SystemPageSize;
        LogFileData = new(PagedPool) UCHAR[LogFileDataSize];
        if (!LogFileData)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Done;
        }

        BytesRemaining = LogFileDataSize;
        Status = LogFile->CopyData(FileDataAttr,
                                   LogFileData,
                                   &BytesRemaining);
        if (!NT_SUCCESS(Status) || BytesRemaining != 0)
            goto Done;
    }

    // Set the restart page pointers.
    RestartPage1 = (PLfsRestartPage)LogFileData;
    RestartPage2 = (PLfsRestartPage)(LogFileData + SystemPageSize);

    RestartPage1Valid =
        NT_SUCCESS(NtfsApplyFixup(
            reinterpret_cast<PNTFSRecordHeader>(RestartPage1),
            SystemPageSize,
            DiskVolume->BytesPerSector)) &&
        IsValidRestartPage(RestartPage1, SystemPageSize);
    RestartPage2Valid =
        NT_SUCCESS(NtfsApplyFixup(
            reinterpret_cast<PNTFSRecordHeader>(RestartPage2),
            SystemPageSize,
            DiskVolume->BytesPerSector)) &&
        IsValidRestartPage(RestartPage2, SystemPageSize);
    if (!RestartPage1Valid && !RestartPage2Valid)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    if (!RestartPage1Valid)
        SelectedPage = RestartPage2;
    else if (!RestartPage2Valid)
        SelectedPage = RestartPage1;
    else
        SelectedPage = GetRestartPageLsn(RestartPage2) >
                       GetRestartPageLsn(RestartPage1)
                       ? RestartPage2
                       : RestartPage1;

    ClientMajorVersion = SelectedPage->MajorVersion;
    ClientMinorVersion = SelectedPage->MinorVersion;

    if (!IsSupportedClientVersion())
    {
        DPRINT1("Client version not supported! (%ld.%ld)\n", ClientMajorVersion, ClientMinorVersion);
        RestartPage1 = NULL;
        RestartPage2 = NULL;
        delete[] LogFileData;
        LogFileData = NULL; // The destructor frees it too.
        Status = STATUS_LOG_BLOCK_VERSION;
        goto Done;
    }

    // Perform file system recovery.
    Status = PerformFileSystemRecovery();

    if (!NT_SUCCESS(Status))
        DPRINT1("Failed to perform self-healing!\n");

Done:
    delete LogFile;
    LogFile = NULL;
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
