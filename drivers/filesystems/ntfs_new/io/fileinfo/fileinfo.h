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

#define GetWStrLength(x) x * sizeof(WCHAR)
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))
#define ULONG_ROUND_UP(x)   ROUND_UP((x), (sizeof(ULONG)))

static
NTSTATUS
GetFileBasicInformation(_In_ PFileContextBlock FileCB,
                        _Out_ PFILE_BASIC_INFORMATION Buffer,
                        _Inout_ PULONG Length)
{
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

    DPRINT1("Getting file standard information...\n");

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

    DPRINT1("Getting file name information...\n");

    // If buffer can't hold the File Name Information struct, fail.
    if (*Length < FileNameInfoSize)
        return STATUS_BUFFER_TOO_SMALL;

    // Save file name length, and as much file len, as buffer length allows.
    Buffer->FileNameLength = wcslen(FileCB->FileName) * sizeof(WCHAR);

    // Calculate amount of bytes to copy not to overflow the buffer.
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

// TODO: Ensure we don't overrun buffer.
static
NTSTATUS
GetFileBothDirectoryInformation(_In_ PFileContextBlock FileCB,
                                _In_ PVolumeContextBlock VolCB,
                                _In_ BOOLEAN ReturnSingleEntry,
                                _Out_ PFILE_BOTH_DIR_INFORMATION Buffer,
                                _Inout_ PULONG Length)
{
    NTSTATUS Status;
    PBTree NewTree;
    PBTreeKey CurrentKey;
    PFileNameEx FileNameData;
    PFILE_BOTH_DIR_INFORMATION BufferPtr;
    ULONG KeysInNode, SizeOfStruct;

    if (ReturnSingleEntry)
        DPRINT1("Return Single Entry is TRUE!\n");
    else
        DPRINT1("Return Single Entry is FALSE!\n");

    // TODO: If not root directory, also return . and .. directories!
    VolCB->Volume->SuperMegaHack++;

    if(VolCB->Volume->SuperMegaHack >= 3)
    {
        DPRINT1("SuperMega Hack is off!\n");
        VolCB->Volume->SuperMegaHack = 0;
        return STATUS_NO_MORE_FILES;
    }

    DPRINT1("SuperMega Hack is ON!\n");
    ASSERT(Buffer);

    PrintFileContextBlock(FileCB);

    // Let's get the btree for this file
    NewTree = NULL;
    Status = CreateBTreeFromFile(FileCB->FileRec, &NewTree);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get BTree!\n");
        return Status;
    }

    DPRINT1("Let's check out the Btree...\n");
    DumpBTree(NewTree);

    // HACK! Just jump to first child node
    CurrentKey = NewTree->RootNode->FirstKey->LesserChild->FirstKey;
    KeysInNode = NewTree->RootNode->FirstKey->LesserChild->KeyCount;
    BufferPtr = Buffer;

    // Hack for only returning one file if requested
    if (ReturnSingleEntry)
        KeysInNode = 1;

    DPRINT1("Searching BTree!\n");
    DPRINT1("Key count: %lu\n", KeysInNode);

    for (int i = 0; i < KeysInNode; i++)
    {
        DPRINT1("Reading key %i...\n", i);

        if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_END)
        {
            // This is a dummy key.
            DPRINT1("Got dummy key!\n");
            __debugbreak();
            break;
        }

        FileNameData = &(CurrentKey->IndexEntry->FileName);
        PrintFilenameAttrHeader(FileNameData);

        BufferPtr->FileIndex = 0; // NOTE: Undefined for NTFS
        BufferPtr->CreationTime.QuadPart = FileNameData->CreationTime;
        BufferPtr->LastAccessTime.QuadPart = FileNameData->LastAccessTime;
        BufferPtr->LastWriteTime.QuadPart = FileNameData->LastWriteTime;
        BufferPtr->ChangeTime.QuadPart = FileNameData->ChangeTime;
        BufferPtr->EndOfFile.QuadPart = FileNameData->DataSize;
        BufferPtr->AllocationSize.QuadPart = FileNameData->AllocatedSize;
        BufferPtr->FileAttributes = FileNameData->Flags;
        BufferPtr->FileNameLength = GetWStrLength(FileNameData->NameLength);
        BufferPtr->EaSize = FileNameData->Extended.EAInfo.PackedEASize;
        BufferPtr->ShortNameLength = 14;
        RtlCopyMemory(BufferPtr->ShortName, L"NOT.IMP", 14);
        RtlCopyMemory(BufferPtr->FileName,
                      FileNameData->Name,
                      GetWStrLength(FileNameData->NameLength));

        if (FileNameData->Flags & FN_DIRECTORY)
            BufferPtr->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

        // Calculate the size of the structure we just made
        SizeOfStruct = sizeof(FILE_BOTH_DIR_INFORMATION) +
                       GetWStrLength(FileNameData->NameLength);

        // Adjust length
        *Length -= SizeOfStruct;

        /* We have to be smart with setting the next entry offset.
         * If the next entry is a dummy key, the next entry offset should be 0.
         * If not, it should be the size of the structure we just made.
         */

        if (i == KeysInNode - 1 ||
            CurrentKey->NextKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_END)
        {
            // The next key is a dummy key.
            BufferPtr->NextEntryOffset = 0;
            break;
        }

        else
        {
            // The next key is valid.
            BufferPtr->NextEntryOffset = SizeOfStruct;

            // Move up to next entry
            BufferPtr = (PFILE_BOTH_DIR_INFORMATION)(((char*)BufferPtr) +
                                                     (SizeOfStruct));

            // Go to the next key
            CurrentKey = CurrentKey->NextKey;
        }
    }

    return Status;
}

static
NTSTATUS
GetFileNetworkOpenInformation(_In_ PFileContextBlock FileCB,
                              _Out_ PFILE_NETWORK_OPEN_INFORMATION Buffer,
                              _Inout_ PULONG Length)
{
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