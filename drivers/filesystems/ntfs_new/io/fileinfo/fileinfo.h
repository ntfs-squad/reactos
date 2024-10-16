/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */
#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

static
NTSTATUS
GetFileBasicInformation(_In_ PFileContextBlock FileCB,
                        _Out_ PFILE_BASIC_INFORMATION Buffer,
                        _Inout_ PULONG Length)
{
    if (!FileCB)
        return STATUS_INVALID_PARAMETER;

    if (*Length < sizeof(FILE_BASIC_INFORMATION))
        return STATUS_BUFFER_TOO_SMALL;

    Buffer->CreationTime = FileCB->CreationTime;
    Buffer->LastAccessTime = FileCB->LastAccessTime;
    Buffer->LastWriteTime = FileCB->LastWriteTime;
    Buffer->ChangeTime = FileCB->ChangeTime;
    Buffer->FileAttributes = FileCB->FileAttributes;

    *Length -= sizeof(FILE_BASIC_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileStandardInformation(_In_ PFileContextBlock FileCB,
                           _Out_ PFILE_STANDARD_INFORMATION Buffer,
                           _Inout_ PULONG Length)
{
    size_t FileInfoSize = sizeof(FILE_STANDARD_INFORMATION);

    if (*Length < FileInfoSize)
        return STATUS_BUFFER_TOO_SMALL;

    if (!FileCB)
        return STATUS_NOT_FOUND;

    Buffer->Directory = FileCB->IsDirectory;
    Buffer->AllocationSize = FileCB->AllocationSize;
    Buffer->EndOfFile = FileCB->EndOfFile;
    Buffer->NumberOfLinks = FileCB->NumberOfLinks;
    Buffer->DeletePending = FileCB->DeletePending;

    *Length -= FileInfoSize;

    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileNameInformation(_In_ PFileContextBlock FileCB,
                       _Out_ PFILE_NAME_INFORMATION Buffer,
                       _Inout_ PULONG Length)
{
    ULONG BytesToCopy;
    size_t FileNameInfoSize = sizeof(FILE_NAME_INFORMATION);

    // If buffer can't hold the File Name Information struct, fail.
    if (*Length < FileNameInfoSize)
        return STATUS_BUFFER_TOO_SMALL;

    // Save file name length, and as much file len, as buffer length allows.
    Buffer->FileNameLength = wcslen(FileCB->FileName) * sizeof(WCHAR);

    // Calculate amount of bytes to copy not to overflow the buffer.
    // TODO: Determine if we need this
    if (*Length < Buffer->FileNameLength + FileNameInfoSize)
    {
        // The buffer isn't big enough. Fill what you can.
        BytesToCopy = *Length - FileNameInfoSize;
        RtlCopyMemory(Buffer->FileName, FileCB->FileName, BytesToCopy);
        *Length = 0;
        return STATUS_BUFFER_OVERFLOW;
    }

    else
    {
        // The buffer is big enough. Fill with file name.
        BytesToCopy = Buffer->FileNameLength;
        RtlCopyMemory(Buffer->FileName, FileCB->FileName, BytesToCopy);
        *Length -= FileNameInfoSize + BytesToCopy;
        return STATUS_SUCCESS;
    }
}

static
NTSTATUS
GetFileInternalInformation(_In_ PFileContextBlock FileCB,
                           _Out_ PFILE_INTERNAL_INFORMATION Buffer,
                           _Inout_ PULONG Length)
{
    if (*Length < sizeof(FILE_INTERNAL_INFORMATION))
        return STATUS_BUFFER_TOO_SMALL;

    Buffer->IndexNumber.QuadPart = FileCB->FileRecordNumber;

    *Length -= sizeof(FILE_INTERNAL_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileNetworkOpenInformation(_In_ PFileContextBlock FileCB,
                              _Out_ PFILE_NETWORK_OPEN_INFORMATION Buffer,
                              _Inout_ PULONG Length)
{
    ASSERT(Buffer);
    ASSERT(FileCB);

    Buffer->CreationTime = FileCB->CreationTime;
    Buffer->LastAccessTime = FileCB->LastAccessTime;
    Buffer->LastWriteTime = FileCB->LastWriteTime;
    Buffer->ChangeTime = FileCB->ChangeTime;
    Buffer->AllocationSize = FileCB->AllocationSize;
    Buffer->EndOfFile = FileCB->EndOfFile;
    Buffer->FileAttributes = FileCB->FileAttributes;

    *Length -= (sizeof(PFILE_NETWORK_OPEN_INFORMATION));

    return STATUS_SUCCESS;
}