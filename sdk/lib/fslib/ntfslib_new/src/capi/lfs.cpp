/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for LFS class
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

VOID
NtfsLogFileServiceGetClientVersion(
    _In_ PNtfsLogFileService Lfs,
    _Out_ PNtfsLfsClientVersion ClientVersion)
{
    // Don't return a pointer to the real client version struct, use a copy instead.
    ClientVersion->MajorVersion = reinterpret_cast<PLogFileService>(Lfs)->ClientVersion.MajorVersion;
    ClientVersion->MinorVersion = reinterpret_cast<PLogFileService>(Lfs)->ClientVersion.MinorVersion;
}

#ifdef __cplusplus
}
#endif
