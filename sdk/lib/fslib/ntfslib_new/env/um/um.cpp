/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Usermode glue
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include <windows.h>
#include <ntfs_um.h>
#include <debug.h>

// Hack: we shouldn't be defining these status codes.
#define STATUS_SUCCESS                   1
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)

HANDLE VolumeHandle = NULL;
ULONG SectorSize = 0;

NTSTATUS
NtfsDiskInitializeUm(
    _In_      HANDLE FileHandle,
    _Out_opt_ ULONG* BytesPerSector)
{
    BOOL Result;
    DISK_GEOMETRY DiskGeometry;
    VolumeHandle = FileHandle;
    ULONG BytesReturned;

    if (!VolumeHandle || VolumeHandle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_PARAMETER;
    
    Result = DeviceIoControl(VolumeHandle,
                             IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             NULL,
                             0,
                             &DiskGeometry,
                             sizeof(DiskGeometry),
                             &BytesReturned,
                             NULL);
    
    if (!Result)
        return STATUS_INVALID_DEVICE_REQUEST;

    SectorSize = DiskGeometry.BytesPerSector;

    if (BytesPerSector)
        *BytesPerSector = SectorSize;

    return STATUS_SUCCESS;
}

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    // Hack: We're assuming buffering is used in readfile below.
    if (!ReadFile(VolumeHandle,
                  Buffer,
                  Length,
                  NULL,
                  NULL))
                  return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer)
{
    UNREFERENCED_PARAMETER(Offset);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Buffer);

    return STATUS_NOT_IMPLEMENTED;
}
