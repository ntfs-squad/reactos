/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

typedef class LogFileService
{
public:
    LogFileService(_In_ PNTFSVolume TargetVolume);
    NTSTATUS LogTransaction();
    NTSTATUS CommitTransaction();
private:
    PNTFSVolume Volume;
    PFileRecord LogFile;
    ULONG ClientMajorVersion;
    ULONG ClientMinorVersion;

    // Call when creating LFS Object
    NTSTATUS PerformFileSystemRecovery();

    // Call every 5 seconds
    NTSTATUS WriteCheckpointRecord();

    // Set in registry with fsutil
    BOOLEAN BugCheckOnCorrupt;
} *PLogFileService;
