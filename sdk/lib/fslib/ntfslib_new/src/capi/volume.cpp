/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for Volume class
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"

#ifdef __cplusplus
extern "C" {
#endif

PNtfsMasterFileTable
NtfsVolumeGetMft(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PNtfsMasterFileTable>(
        reinterpret_cast<PVolume>(DiskVolume)->MFT);
}

#ifdef __cplusplus
}
#endif
