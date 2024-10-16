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

#define IsLastEntry(Key) !!(Key->NextKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_END) && !(Key->NextKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)

static
NTSTATUS
AddKeyToBothDirInfo(_In_    PBTreeKey Key,
                    _In_    BOOLEAN IsLastEntry,
                    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                    _Inout_ PULONG BufferLength,
                    _Out_   PULONG EntryLength)
{
    PFileNameEx FileNameData;
    ULONG EntrySize;

    // Set the file name data pointer
    FileNameData = &(Key->IndexEntry->FileName);

    Buffer->FileIndex = 0;                                            // Undefined for NTFS
    Buffer->CreationTime.QuadPart = FileNameData->CreationTime;
    Buffer->LastAccessTime.QuadPart = FileNameData->LastAccessTime;
    Buffer->LastWriteTime.QuadPart = FileNameData->LastWriteTime;
    Buffer->ChangeTime.QuadPart = FileNameData->ChangeTime;
    Buffer->EndOfFile.QuadPart = FileNameData->DataSize;
    Buffer->AllocationSize.QuadPart = FileNameData->AllocatedSize;
    Buffer->FileAttributes = FileNameData->Flags;
    Buffer->FileNameLength = GetWStrLength(FileNameData->NameLength);
    Buffer->EaSize = FileNameData->Extended.EAInfo.PackedEASize;
    RtlCopyMemory(Buffer->FileName,
                  FileNameData->Name,
                  GetWStrLength(FileNameData->NameLength));

    // Mark file as folder if it is a directory
    if (FileNameData->Flags & FN_DIRECTORY)
        Buffer->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

    // Let's get the short name
    RtlZeroMemory(Buffer->ShortName, MAX_SHORTNAME_LENGTH * sizeof(WCHAR));

    if (FileNameData->NameLength <= MAX_SHORTNAME_LENGTH)
    {
        // We don't need a short name.
        Buffer->ShortNameLength = 0;
    }

    else if (GetFRNFromFileRef(FileRef(Key)) == GetFRNFromFileRef(FileRef(Key->NextKey)))
    {
        // Both keys point to the same file. Assert that it is a valid short name.
        ASSERT(Key->NextKey->IndexEntry->FileName.NameLength <= MAX_SHORTNAME_LENGTH);

        // Move to next key
        Key = Key->NextKey;
        FileNameData = &(Key->IndexEntry->FileName);

        // Copy short name data into the buffer
        Buffer->ShortNameLength = GetWStrLength(FileNameData->NameLength);
        RtlCopyMemory(Buffer->ShortName,
                      FileNameData->Name,
                      GetWStrLength(FileNameData->NameLength));
    }

    else
    {
        // The short name is not the next key in the btree. Something is wrong.
        __debugbreak();
    }

    /* Set the entry size.
     * Note: Entries in the buffer must be aligned to 8-byte boundaries
     * See: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/270df317-9ba5-4ccb-ba00-8d22be139bc5
     */
    EntrySize = ULONG_ROUND_UP(sizeof(FILE_BOTH_DIR_INFORMATION) +
                               GetWStrLength(FileNameData->NameLength));
    *BufferLength -= EntrySize;

    // Set next entry offset
    if (IsLastEntry)
        Buffer->NextEntryOffset = 0;
    else
        Buffer->NextEntryOffset = EntrySize;

    if (EntryLength)
        *EntryLength = EntrySize;

    return STATUS_SUCCESS;
}

// TODO: Ensure we don't overrun buffer.
static
NTSTATUS
AddFirstNodeEntry(_In_    PBTreeFilenameNode Node,
                  _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                  _Inout_ PULONG BufferLength)
{
    PBTreeKey CurrentKey;

    // Find the first key we will copy
    CurrentKey = Node->FirstKey;
    if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_END)
    {
        if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
        {
            // This key is an index node. Check it for the first key.
            return AddFirstNodeEntry(CurrentKey->LesserChild, Buffer, BufferLength);
        }
        else
        {
            // This is an empty node.
            DPRINT1("Empty node!\n");
            return STATUS_NOT_FOUND;
        }
    }

    return AddKeyToBothDirInfo(CurrentKey, TRUE, Buffer, BufferLength, NULL);
}

// TODO: Ensure we don't overrun buffer.
static
NTSTATUS
AddNodeEntry(_In_    PBTreeFilenameNode Node,
             _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
             _Inout_ PULONG BufferLength)
{
    NTSTATUS Status;
    PBTreeKey CurrentKey;
    ULONG EntrySize;

    // Start the search with the first key
    CurrentKey = Node->FirstKey;
    EntrySize = 0;

    while(CurrentKey)
    {
        if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_NODE)
        {
            // Add the contents of the index node.
            Status = AddNodeEntry(CurrentKey->LesserChild, Buffer, BufferLength);
        }

        if (CurrentKey->IndexEntry->Flags & NTFS_INDEX_ENTRY_END)
        {
            // We've reached the end of this node including any index nodes.
            DPRINT1("Reached end of node!\n");
            break;
        }

        // Add this key to the buffer
        Status = AddKeyToBothDirInfo(CurrentKey, IsLastEntry(CurrentKey), Buffer, BufferLength, &EntrySize);
        ASSERT(EntrySize);

        // Increment the buffer
        Buffer = (PFILE_BOTH_DIR_INFORMATION)((ULONG_PTR)Buffer + EntrySize);

        // Go to the next key
        CurrentKey = CurrentKey->NextKey;
    }

    return Status;
}

// TODO: How do we figure out where to start?
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
    PFILE_BOTH_DIR_INFORMATION BufferStart;

    if (ReturnSingleEntry)
        DPRINT1("Return Single Entry is TRUE!\n");
    else
        DPRINT1("Return Single Entry is FALSE!\n");

    // TODO: If not root directory, also return . and .. directories maybe?
    VolCB->Volume->SuperMegaHack++;

    if(VolCB->Volume->SuperMegaHack >= 3)
    {
        DPRINT1("SuperMega Hack is off!\n");
        VolCB->Volume->SuperMegaHack = 0;
        return STATUS_NO_MORE_FILES;
    }

    DPRINT1("SuperMega Hack is ON!\n");
    ASSERT(Buffer);

    // Let's get the btree for this file
    NewTree = NULL;
    Status = CreateBTreeFromFile(FileCB->FileRec, &NewTree);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get BTree!\n");
        return Status;
    }

    /* Populate the buffer.
     * Note: Because some keys can be index nodes, this must be done recursively.
     * These recursive functions modify the buffer pointer, so we have to restore it.
     */
    BufferStart = Buffer;
    if (ReturnSingleEntry)
        Status = AddFirstNodeEntry(NewTree->RootNode, Buffer, Length);
    else
        Status = AddNodeEntry(NewTree->RootNode, Buffer, Length);

    // Set last buffer entry to have a zero next index
    // Buffer->NextEntryOffset = 0;

    // Reset the buffer to the beginning.
    Buffer = BufferStart;

    return Status;
}