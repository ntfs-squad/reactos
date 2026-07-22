/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for Volume class
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shows the super hidden NTFS metadata files in directory listings. */
BOOLEAN NtfsShowMetadataFiles = FALSE;

void
NtfsSetShowMetadataFiles(
    _In_ BOOLEAN Show)
{
    NtfsShowMetadataFiles = Show;
}

NTSTATUS
NtfsVolumeGetADSPreference(
    _In_ PNtfsVolume DiskVolume,
    _In_ PUNICODE_STRING FileName,
    _Out_ AttributeType* RequestedType,
    _Out_ PWSTR* RequestedStream)
{
    return reinterpret_cast<PVolume>(DiskVolume)->GetADSPreference(FileName,
                                                                   RequestedType,
                                                                   RequestedStream);
}

UINT8
NtfsVolumeGetBytesPerSector(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->BytesPerSector;
}

ULONG
NtfsVolumeGetClustersInVolume(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->ClustersInVolume;
}

NTSTATUS
NtfsVolumeGetFreeClusters(
    _In_ PNtfsVolume DiskVolume,
    _Out_ PLARGE_INTEGER FreeClusters)
{
    return reinterpret_cast<PVolume>(DiskVolume)->GetFreeClusters(FreeClusters);
}

PNtfsLogFileService
NtfsVolumeGetLFS(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PNtfsLogFileService>(
        reinterpret_cast<PVolume>(DiskVolume)->LFS);
}

PNtfsMasterFileTable
NtfsVolumeGetMft(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PNtfsMasterFileTable>(
        reinterpret_cast<PVolume>(DiskVolume)->MFT);
}

USHORT
NtfsVolumeGetMajorVersion(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->NtfsMajorVersion;
}

USHORT
NtfsVolumeGetMinorVersion(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->NtfsMinorVersion;
}

UINT8
NtfsVolumeGetSectorsPerCluster(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->SectorsPerCluster;
}

UINT64
NtfsVolumeGetSerialNumber(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->SerialNumber;
}

NTSTATUS
NtfsVolumeGetVolumeLabel(
    _In_ PNtfsVolume DiskVolume,
    _Out_ PWSTR VolumeLabel,
    _Out_ PUSHORT Length)
{
    return reinterpret_cast<PVolume>(DiskVolume)->GetVolumeLabel(VolumeLabel, Length);
}

BOOLEAN
NtfsVolumeIsReadOnly(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PVolume>(DiskVolume)->IsReadOnly;
}

NTSTATUS
NtfsVolumeSetVolumeLabel(
    _In_ PNtfsVolume DiskVolume,
    _In_ PWSTR VolumeLabel,
    _In_ ULONG Length)
{
    return reinterpret_cast<PVolume>(DiskVolume)->SetVolumeLabel(VolumeLabel, Length);
}

#ifdef __cplusplus
}
#endif
