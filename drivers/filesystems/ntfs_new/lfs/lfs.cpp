/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Log File Service (LFS) Implementation
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

LogFileService::LogFileService(_In_ PNTFSVolume TargetVolume)
{
    // Store volume pointer
    Volume = TargetVolume;
}