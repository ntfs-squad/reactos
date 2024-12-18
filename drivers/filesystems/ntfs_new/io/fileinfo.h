/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

// NOTE: We don't include MFT space in our allocation size, similar to Windows.

static
NTSTATUS
GetFileBasicInformation(_In_ PFileContextBlock FileCB,
                        _Out_ PFILE_BASIC_INFORMATION Buffer,
                        _Inout_ PULONG Length)
{
    PFileRecord File;
    PStandardInformationEx StdInfo;

    if (!FileCB)
        return STATUS_INVALID_PARAMETER;

    if (*Length < sizeof(FILE_BASIC_INFORMATION))
        return STATUS_BUFFER_TOO_SMALL;

    File = FileCB->FileRec;

    // From $STANDARD_INFORMATION
    StdInfo = (PStandardInformationEx)
              GetResidentDataPointer(File->GetAttribute(TypeStandardInformation,
                                     NULL));

    Buffer->CreationTime.QuadPart = StdInfo->CreationTime;
    Buffer->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
    Buffer->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
    Buffer->ChangeTime.QuadPart = StdInfo->ChangeTime;
    Buffer->FileAttributes = StdInfo->FilePermissions;

    *Length -= sizeof(FILE_BASIC_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileStandardInformation(_In_ PFileContextBlock FileCB,
                           _Out_ PFILE_STANDARD_INFORMATION Buffer,
                           _Inout_ PULONG Length)
{
    PFileRecord File;
    PAttribute DataAttribute;
    size_t FileInfoSize = sizeof(FILE_STANDARD_INFORMATION);

    if (*Length < FileInfoSize)
        return STATUS_BUFFER_TOO_SMALL;

    if (!FileCB)
        return STATUS_NOT_FOUND;

    File = FileCB->FileRec;

    // Information from $DATA
    DataAttribute = File->GetAttribute(TypeData,
                                       NULL);

    if (DataAttribute)
    {
        if (DataAttribute->IsNonResident)
        {
            Buffer->EndOfFile.QuadPart = DataAttribute->NonResident.DataSize;
            Buffer->AllocationSize.QuadPart = DataAttribute->NonResident.AllocatedSize;
        }

        else
        {
            Buffer->EndOfFile.QuadPart = DataAttribute->Resident.DataLength;
            Buffer->AllocationSize.QuadPart = 0;
        }
    }

    else
    {
        Buffer->EndOfFile.QuadPart = 0;
        Buffer->AllocationSize.QuadPart = 0;
    }

    // Information from file header
    Buffer->Directory = !!(File->Header->Flags & FR_IS_DIRECTORY);
    Buffer->NumberOfLinks = File->Header->HardLinkCount;

    // Information from file context block
    Buffer->DeletePending = !!(FileCB->CreateOptions & FILE_DELETE_ON_CLOSE);

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
    Buffer->FileNameLength = FileCB->FileName.Length;
    // Calculate amount of bytes to copy not to overflow the buffer.
    // TODO: Determine if we need this
    if (*Length < Buffer->FileNameLength + FileNameInfoSize)
    {
        // The buffer isn't big enough. Fill what you can.
        BytesToCopy = *Length - FileNameInfoSize;
        RtlCopyMemory(Buffer->FileName, FileCB->FileName.Buffer, BytesToCopy);
        *Length = 0;
        return STATUS_BUFFER_OVERFLOW;
    }

    else
    {
        // The buffer is big enough. Fill with file name.
        BytesToCopy = Buffer->FileNameLength;
        RtlCopyMemory(Buffer->FileName, FileCB->FileName.Buffer, BytesToCopy);
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

    /* From Microsoft Learn:
     * The FILE_INTERNAL_INFORMATION structure is used to query for the file
     * system's 8-byte file reference number for a file.
     */

    if (*Length < sizeof(FILE_INTERNAL_INFORMATION))
        return STATUS_BUFFER_TOO_SMALL;

    Buffer->IndexNumber.QuadPart = FileCB->FileRec->Header->MFTRecordNumber;

    *Length -= sizeof(FILE_INTERNAL_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileNetworkOpenInformation(_In_ PFileContextBlock FileCB,
                              _Out_ PFILE_NETWORK_OPEN_INFORMATION Buffer,
                              _Inout_ PULONG Length)
{
    PAttribute DataAttribute;
    PStandardInformationEx StdInfo;
    PFileRecord File;

    ASSERT(Buffer);
    ASSERT(FileCB);

    File = FileCB->FileRec;

    // Information from $DATA
    DataAttribute = File->GetAttribute(TypeData,
                                       NULL);

    if (DataAttribute)
    {
        if (DataAttribute->IsNonResident)
        {
            Buffer->EndOfFile.QuadPart = DataAttribute->NonResident.DataSize;
            Buffer->AllocationSize.QuadPart = DataAttribute->NonResident.AllocatedSize;
        }

        else
        {
            Buffer->EndOfFile.QuadPart = DataAttribute->Resident.DataLength;
            Buffer->AllocationSize.QuadPart = 0;
        }
    }

    else
    {
        Buffer->EndOfFile.QuadPart = 0;
        Buffer->AllocationSize.QuadPart = 0;
    }

    File = FileCB->FileRec;

    // From $STANDARD_INFORMATION
    StdInfo = (PStandardInformationEx)
              GetResidentDataPointer(File->GetAttribute(TypeStandardInformation,
                                     NULL));

    Buffer->CreationTime.QuadPart = StdInfo->CreationTime;
    Buffer->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
    Buffer->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
    Buffer->ChangeTime.QuadPart = StdInfo->ChangeTime;
    Buffer->FileAttributes = StdInfo->FilePermissions;

    *Length -= (sizeof(PFILE_NETWORK_OPEN_INFORMATION));

    return STATUS_SUCCESS;
}