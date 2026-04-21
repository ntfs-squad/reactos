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

NTSTATUS
NtfsVolumeGetADSPreference(
    _In_ PNtfsVolume DiskVolume,
    _In_ PFILE_OBJECT FileObject,
    _Out_ AttributeType* RequestedType,
    _Out_ PWSTR* RequestedStream)
{
    return reinterpret_cast<PVolume>(DiskVolume)->GetADSPreference(reinterpret_cast<PFILE_OBJECT>(FileObject),
                                                                   RequestedType,
                                                                   RequestedStream);
}

BOOLEAN
NtfsVolumeIsReadOnly(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->IsReadOnly;
}

#ifdef __cplusplus
}
#endif
