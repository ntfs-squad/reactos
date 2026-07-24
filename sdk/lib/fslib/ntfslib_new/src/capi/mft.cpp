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
NtfsMasterFileTableGetFileRecordFromQueryEx(
    _In_ PNtfsMasterFileTable Mft,
    _In_reads_(QueryLength) PWCHAR Query,
    _In_ ULONG QueryLength,
    _In_ BOOLEAN OpenFinalReparsePoint,
    _Out_ PULONG RemainingNameLength,
    _Out_ PNtfsFileRecord* File)
{
    PWCHAR TerminatedQuery;
    NTSTATUS Status;

    if (!Mft || !Query ||
        !RemainingNameLength || !File)
        return STATUS_INVALID_PARAMETER;
    *RemainingNameLength = 0;
    *File = NULL;
    if (QueryLength >
        MAXUSHORT / sizeof(WCHAR))
    {
        return STATUS_NAME_TOO_LONG;
    }

    TerminatedQuery =
        new(PagedPool, TAG_NTFS)
            WCHAR[(SIZE_T)QueryLength + 1];
    if (!TerminatedQuery)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(TerminatedQuery,
                  Query,
                  QueryLength * sizeof(WCHAR));
    TerminatedQuery[QueryLength] = 0;

    Status = reinterpret_cast<PMasterFileTable>(Mft)->
        GetFileRecordFromQuery(
            TerminatedQuery,
            reinterpret_cast<PFileRecord*>(File),
            TRUE,
            OpenFinalReparsePoint,
            RemainingNameLength);
    delete[] TerminatedQuery;
    return Status;
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
