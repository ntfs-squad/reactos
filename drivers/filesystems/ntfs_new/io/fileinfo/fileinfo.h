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
#if 0
    size_t FileInfoSize = sizeof(FILE_BASIC_INFORMATION);

    DPRINT1("Getting file basic information...\n");

    if (*Length < FileInfoSize)
        return STATUS_BUFFER_TOO_SMALL;

    RtlZeroMemory(Buffer, FileInfoSize);

    // Make something up for now.
    UNREFERENCED_PARAMETER(FileCB);
    Buffer->CreationTime.QuadPart = 1;
    Buffer->LastAccessTime.QuadPart = 1;
    Buffer->LastWriteTime.QuadPart = 1;
    Buffer->ChangeTime.QuadPart = 1;
    Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

    *Length -= FileInfoSize;

    return STATUS_SUCCESS;
#else
    UNREFERENCED_PARAMETER(FileCB);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static
NTSTATUS
GetFileNameInformation(_In_ PFileContextBlock FileCB,
                       _Out_ PFILE_NAME_INFORMATION Buffer,
                       _Inout_ PULONG Length)
{
    ULONG BytesToCopy;
    size_t FileNameInfoSize = sizeof(FILE_NAME_INFORMATION);

    DPRINT1("Getting File Name Information...\n");

    // If buffer can't hold the File Name Information struct, fail.
    if (*Length < FileNameInfoSize)
        return STATUS_BUFFER_TOO_SMALL;

    // Hack to get drive properties to work
    UNREFERENCED_PARAMETER(FileCB);
    WCHAR PathName[4] = L"D:\\";

    // Save file name length, and as much file len, as buffer length allows.
    Buffer->FileNameLength = wcslen(PathName) * sizeof(WCHAR);

    // Calculate amount of bytes to copy not to overflow the buffer.
    if (*Length < Buffer->FileNameLength + FileNameInfoSize)
    {
        // The buffer isn't big enough. Fill what you can.
        BytesToCopy = *Length - FileNameInfoSize;
        RtlCopyMemory(Buffer->FileName, PathName, BytesToCopy);
        *Length = 0;
        return STATUS_BUFFER_OVERFLOW;
    }

    else
    {
        // The buffer is big enough. Fill with file name.
        BytesToCopy = Buffer->FileNameLength;
        RtlCopyMemory(Buffer->FileName, PathName, BytesToCopy);
        *Length -= FileNameInfoSize + BytesToCopy;
        return STATUS_SUCCESS;
    }
}