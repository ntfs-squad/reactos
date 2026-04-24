/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     C interface for FileRecord class
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfs_tags.h"

#ifdef __cplusplus
extern "C" {
#endif

PNtfsFileRecord
NtfsFileRecordCreate(
    _In_ void *Volume,
    _In_ ULONG FileRecordSize)
{
    PVolume Vol = static_cast<PVolume>(Volume);

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
NTAPI
NtfsFileRecordGetHeader(
    _In_ PNtfsFileRecord Fr)
{
    return reinterpret_cast<PFileRecord>(Fr)->Header;
}

PUCHAR
NTAPI
NtfsFileRecordGetData(
    _In_ PNtfsFileRecord Fr)
{
    return reinterpret_cast<PFileRecord>(Fr)->Data;
}

PAttribute
NTAPI
NtfsFileRecordGetAttribute(
    _In_ PNtfsFileRecord Fr,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name)
{
    return reinterpret_cast<PFileRecord>(Fr)->GetAttribute(Type, Name);
}

PDataRun
NTAPI
NtfsFileRecordFindNonResidentDataFromAttribute(
    _In_ PNtfsFileRecord Fr,
    _In_ PAttribute DataAttr)
{
    return reinterpret_cast<PFileRecord>(Fr)->FindNonResidentData(DataAttr);
}

PDataRun
NTAPI
NtfsFileRecordFindNonResidentData(
    _In_ PNtfsFileRecord Fr,
    _In_ AttributeType Type,
    _In_opt_ PWSTR Name)
{
    return reinterpret_cast<PFileRecord>(Fr)->FindNonResidentData(Type, Name);
}

NTSTATUS
NTAPI
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
NTAPI
NtfsFileRecordCopyDataFromAttribute(
    _In_ PNtfsFileRecord Fr,
    _In_ PAttribute Attr,
    _In_ PUCHAR Buffer,
    _Inout_ PULONG Length,
    _In_ ULONGLONG Offset)
{
    return reinterpret_cast<PFileRecord>(Fr)->CopyData(Attr, Buffer, Length, Offset);
}

NTSTATUS
NTAPI
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

NTSTATUS
NTAPI
NtfsFileRecordUpdateResidentData(
    _In_ PNtfsFileRecord Fr,
    _In_ PAttribute TargetAttribute,
    _In_ PUCHAR Buffer,
    _In_ PULONG Length,
    _In_ ULONGLONG Offset)
{
    return reinterpret_cast<PFileRecord>(Fr)->UpdateResidentData(TargetAttribute, Buffer, Length, Offset);
}

NTSTATUS
NTAPI
NtfsFileRecordCommitFixup(
    _In_ PNtfsFileRecord Fr)
{
    return reinterpret_cast<PFileRecord>(Fr)->CommitFixup();
}

NTSTATUS
NTAPI
NtfsFileRecordApplyFixup(
    _In_ PNtfsFileRecord Fr)
{
    return reinterpret_cast<PFileRecord>(Fr)->ApplyFixup();
}

#ifdef __cplusplus
}
#endif

