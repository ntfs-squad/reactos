/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for MFT class
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsMasterFileTableGetFileRecordFromQuery(
    _In_ PNtfsMasterFileTable Mft,
    _In_ PWCHAR Query,
    _Out_ PNtfsFileRecord* File)
{
    return reinterpret_cast<PMasterFileTable>(Mft)->GetFileRecordFromQuery(Query, reinterpret_cast<PFileRecord*>(File));
}

NTSTATUS
NtfsMasterFileTableReadFileRecord(
    _In_ PNtfsMasterFileTable Mft,
    _In_ ULONGLONG RequestedFileReference,
    _Out_ PULONGLONG ReturnedFileReference,
    _Out_opt_ PUCHAR Buffer,
    _Inout_ PULONG BufferLength)
{
    if (!Mft)
        return STATUS_INVALID_PARAMETER;
    return reinterpret_cast<PMasterFileTable>(Mft)->
        ReadFileRecord(RequestedFileReference,
                       ReturnedFileReference,
                       Buffer,
                       BufferLength);
}

#ifdef __cplusplus
}
#endif
