/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

typedef class RestartPage
{
    PLfsRestartPage Header;
    PLfsRestartArea Foo;

    // The head for an array of clients for the log file (usually just NTFS itself)
    PLfsClientRecord ClientArrayHead;
} *PRestartPage;

typedef class LogFileService
{
public:
    ULONG ClientMajorVersion;
    ULONG ClientMinorVersion;

    LogFileService(_In_ PVolume TargetVolume);
    ~LogFileService();
    NTSTATUS InitializeLFS();
    NTSTATUS LogTransaction();
    NTSTATUS CommitTransaction();
    NTSTATUS ShutdownLFS();
private:
    PVolume DiskVolume;
    PFileRecord LogFile;
    PUCHAR LogFileData;

    PLfsRestartPage RestartPage1;
    PLfsRestartPage RestartPage2;

    // Call when creating LFS Object
    NTSTATUS PerformFileSystemRecovery();

    // Call every 5 seconds
    NTSTATUS WriteCheckpointRecord();

    BOOLEAN IsSupportedClientVersion()
    {
        // Supported versions currently include: 0.0, 1.0, 1.1
        if (ClientMajorVersion == 1)
        {
            return ClientMinorVersion == 0
                   || ClientMinorVersion == 1;
        }

        else if (ClientMajorVersion = 0)
        {
            return ClientMinorVersion == 0;
        }

        return FALSE;
    }
} *PLogFileService;
