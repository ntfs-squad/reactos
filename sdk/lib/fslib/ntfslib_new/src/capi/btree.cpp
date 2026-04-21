/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for BTree class and related classes
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"

#ifdef __cplusplus
extern "C" {
#endif

PNtfsDirectory
NtfsDirectoryCreate(
    _In_ PNtfsVolume DiskVolume)
{
    return reinterpret_cast<PNtfsDirectory>(new(NonPagedPool) Directory(reinterpret_cast<PVolume>(DiskVolume)));
}

PNtfsDirectory
NtfsDirectoryCreateEx(
    _In_ PNtfsVolume DiskVolume,
    _In_ PNtfsFileRecord File)
{
    return reinterpret_cast<PNtfsDirectory>(new(NonPagedPool) Directory(reinterpret_cast<PVolume>(DiskVolume),
                                                                        reinterpret_cast<PFileRecord>(File)));
}

NTSTATUS
NTAPI
NtfsDirectoryGetFileBothDirInfo(
    _In_    PNtfsDirectory Dir,
    _In_    BOOLEAN ReturnSingleEntry,
    _In_    BOOLEAN RestartScan,
    _In_    PUNICODE_STRING FileNameFilter,
    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
    _Inout_ PULONG BufferLength)
{
    return reinterpret_cast<PDirectory>(Dir)->GetFileBothDirInfo(ReturnSingleEntry,
                                                                 RestartScan,
                                                                 FileNameFilter,
                                                                 Buffer,
                                                                 BufferLength);
}

NTSTATUS
NtfsDirectoryLoadDirectory(
    _In_ PNtfsDirectory Dir,
    _In_ PNtfsFileRecord File)
{
    return reinterpret_cast<PDirectory>(Dir)->LoadDirectory(reinterpret_cast<PFileRecord>(File));
}

#ifdef __cplusplus
}
#endif
