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
NtfsMasterFileTableGetFileRecord(
    _In_  PNtfsMasterFileTable Mft,
    _In_  ULONG FileRecordNumber,
    _Out_ PNtfsFileRecord* File)
{
    return reinterpret_cast<PMasterFileTable>(Mft)->GetFileRecord(FileRecordNumber, reinterpret_cast<PFileRecord*>(File));
}

NTSTATUS
NtfsMasterFileTableGetFileRecordFromQuery(
    _In_ PNtfsMasterFileTable Mft,
    _In_ PWCHAR Query,
    _Out_ PNtfsFileRecord* File)
{
    return reinterpret_cast<PMasterFileTable>(Mft)->GetFileRecordFromQuery(Query, reinterpret_cast<PFileRecord*>(File));
}

NTSTATUS
NtfsMasterFileTableGetMftInformation(
    _In_ PNtfsMasterFileTable Mft,
    _Out_ PNtfsMasterFileTableInfo MftInformation)
{
    MftInformation->MFTLCN = reinterpret_cast<PMasterFileTable>(Mft)->MFTLCN;
    MftInformation->MFTMirrLCN = reinterpret_cast<PMasterFileTable>(Mft)->MFTMirrLCN;
    MftInformation->MFTZoneEndLCN = reinterpret_cast<PMasterFileTable>(Mft)->MFTZoneEndLCN;

    return STATUS_SUCCESS;
}

#ifdef __cplusplus
}
#endif
