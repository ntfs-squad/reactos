/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for FileRecord class
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

PNtfsFileRecord
NtfsFileRecordCreate(
    _In_ PNtfsVolume DiskVolume,
    _In_ ULONG FileRecordSize)
{
    PVolume Vol = reinterpret_cast<PVolume>(DiskVolume);

    if (!Vol)
        return nullptr;

    if (FileRecordSize == 0)
        return reinterpret_cast<PNtfsFileRecord>(new(NonPagedPool, TAG_FILE_RECORD) FileRecord(Vol));

    return reinterpret_cast<PNtfsFileRecord>(new(NonPagedPool, TAG_FILE_RECORD) FileRecord(Vol, FileRecordSize));
}

void
NtfsFileRecordDestroy(
    _In_opt_ PNtfsFileRecord Fr)
{
    if (!Fr)
        return;
    delete reinterpret_cast<PFileRecord>(Fr);
}

PFileRecordHeader
NtfsFileRecordGetHeader(
    _In_ PNtfsFileRecord Fr)
{
    return reinterpret_cast<PFileRecord>(Fr)->Header;
}

PAttribute
NtfsFileRecordGetAttribute(
    _In_ PNtfsFileRecord Fr,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name)
{
    return reinterpret_cast<PFileRecord>(Fr)->GetAttribute(Type, Name);
}

NTSTATUS
NtfsFileRecordGetAttributeData(
    _In_     PNtfsFileRecord Fr,
    _In_     AttributeType Type,
    _In_opt_ PWSTR Name,
    _Out_    PUCHAR *Data)
{
    return reinterpret_cast<PFileRecord>(Fr)->GetAttributeData(Type, Name, Data);
}

NTSTATUS
NtfsFileRecordCopyData(
    _In_ PNtfsFileRecord Fr,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset)
{
    return reinterpret_cast<PFileRecord>(Fr)->CopyData(Type, Name, Buffer, Length, Offset);
}

NTSTATUS
NtfsFileRecordWriteFileData(
    _In_ PNtfsFileRecord Fr,
    _In_ AttributeType AttrType,
    _In_opt_ PWSTR StreamName,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ PLARGE_INTEGER Offset)
{
    return reinterpret_cast<PFileRecord>(Fr)->WriteFileData(AttrType, StreamName, Buffer, Length, Offset);
}

#ifdef __cplusplus
}
#endif

