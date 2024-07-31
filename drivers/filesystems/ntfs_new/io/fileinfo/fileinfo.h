/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */
#include "../ntfsprocs.h"
#include <debug.h>

static
NTSTATUS
GetFileBasicInformation(_In_ PFileContextBlock FileCB,
                        _Out_ PFILE_BASIC_INFORMATION Buffer,
                        _Inout_ PULONG Length)
{
    size_t FileInfoSize = sizeof(FILE_BASIC_INFORMATION);

    DPRINT1("Geting file basic information...\n");

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

    *Length = FileInfoSize;

    return STATUS_NOT_IMPLEMENTED;
}

static
NTSTATUS
GetFileNameInformation(_In_ PFileContextBlock FileCB,
                       _Out_ PFILE_NAME_INFORMATION Buffer,
                       _Inout_ PULONG Length)
{
    DPRINT1("Getting File Name Information...\n");
    ULONG BytesToCopy;
    size_t FileNameInfoHeaderSize = sizeof(FILE_NAME_INFORMATION);

    // This is a hack to see where this shows up.
    WCHAR PathName[16] = L"Hello World.bin";
    UNREFERENCED_PARAMETER(FileCB);

    // If buffer can't hold the File Name Information struct, fail.
    if (*Length < FileNameInfoHeaderSize)
        return STATUS_BUFFER_TOO_SMALL;

    // Save file name length, and as much file len, as buffer length allows.
    Buffer->FileNameLength = wcslen(PathName) * sizeof(WCHAR);

    // Calculate amount of bytes to copy not to overflow the buffer.
    if (*Length < Buffer->FileNameLength + FileNameInfoHeaderSize)
    {
        BytesToCopy = *Length - FileNameInfoHeaderSize;
        // Fill buffer.
        RtlCopyMemory(Buffer->FileName, PathName, BytesToCopy);
        return STATUS_BUFFER_OVERFLOW;
    }

    else
    {
        BytesToCopy = Buffer->FileNameLength;
        // Fill buffer.
        RtlCopyMemory(Buffer->FileName, PathName, BytesToCopy);
        // We filled up as many bytes, as needed.
        *Length = FileNameInfoHeaderSize + BytesToCopy;
        return STATUS_SUCCESS;
    }
}