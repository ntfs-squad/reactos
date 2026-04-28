/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Usermode glue
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include <windows.h>
#include <ntfs_um.h>

HANDLE VolumeHandle = NULL;
ULONG SectorSize = 0;

NTSTATUS
NtfsDiskInitializeUm(
    _In_ HANDLE FileHandle)
{
    BOOL Result;
    DISK_GEOMETRY DiskGeometry;
    VolumeHandle = FileHandle;

    if (!VolumeHandle || VolumeHandle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_PARAMETER;
    
    Result = DeviceIoControl(VolumeHandle,
                             IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             NULL,
                             0,
                             &DiskGeometry,
                             sizeof(DiskGeometry),
                             NULL,
                             NULL);
    
    if (!Result)
        return STATUS_INVALID_DEVICE_REQUEST;

    SectorSize = DiskGeometry.BytesPerSector;

    return STATUS_SUCCESS;
}

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PUCHAR AlignedBuffer = NULL;
    ULONGLONG AlignedOffset;
    ULONG AlignedLength;
    ULONG Delta;
    DWORD BytesRead;
    LARGE_INTEGER FileOffset;

    AlignedOffset = Offset & ~((ULONGLONG)SectorSize - 1);
    Delta = (ULONG)(Offset - AlignedOffset);

    AlignedLength = Delta + Length;
    AlignedLength = (AlignedLength + (SectorSize - 1)) & ~(SectorSize - 1);

    AlignedBuffer = (PUCHAR)_aligned_malloc(AlignedLength, SectorSize);
    if (AlignedBuffer == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

    FileOffset.QuadPart = AlignedOffset;

    if (!SetFilePointerEx(VolumeHandle, FileOffset, NULL, FILE_BEGIN))
    {
        Status = STATUS_UNSUCCESSFUL;
        goto Cleanup;
    }

    if (!ReadFile(VolumeHandle,
                  AlignedBuffer,
                  AlignedLength,
                  &BytesRead,
                  NULL))
    {
        Status = STATUS_UNSUCCESSFUL;
        goto Cleanup;
    }

    if (BytesRead < Delta + Length)
    {
        Status = STATUS_END_OF_FILE;
        goto Cleanup;
    }

    RtlCopyMemory(Buffer, AlignedBuffer + Delta, Length);

Cleanup:
    if (AlignedBuffer)
        _aligned_free(AlignedBuffer);

    return Status;
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
