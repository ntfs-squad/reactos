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
private:
    PNTFSVolume Volume;
} *PLogFileService;
