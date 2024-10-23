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

#define IsLastEntry(Key) !!(Key->Entry->Flags & INDEX_ENTRY_END)

#define IsIndexNode(Key) !!(Key->Entry->Flags & INDEX_ENTRY_NODE)

#define IsEndOfNode(Key) IsLastEntry(Key) && !IsIndexNode(Key)

#define GetNextKey(Key) \
IsIndexNode(Key) ? Key->ChildNode->FirstKey : IsEndOfNode(Key) ? \
Key->ParentNodeKey ? Key->ParentNodeKey->NextKey : NULL : Key->NextKey

static
NTSTATUS
AddKeyToBothDirInfo(_In_    PBTreeKey *Key,
                    _In_    BOOLEAN IsLastEntry,
                    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                    _Inout_ PULONG BufferLength,
                    _Out_   PULONG EntryLength = NULL)
{
    PFileNameEx FileNameData;
    ULONG EntrySize;

    // Set the file name data pointer
    FileNameData = GetFileName(*Key);
    EntrySize = ULONG_ROUND_UP(sizeof(FILE_BOTH_DIR_INFORMATION) + GetWStrLength(FileNameData->NameLength));

    if (*BufferLength < EntrySize)
    {
        // We will overrun the buffer if we continue
        DPRINT1("Unable to add key to buffer: too small!\n");
        __debugbreak();
        return STATUS_BUFFER_TOO_SMALL;
    }

    Buffer->FileIndex = 0; // Undefined for NTFS
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

    else if (GetFRNFromFileRef(FileRef((*Key))) == GetFRNFromFileRef(FileRef((*Key)->NextKey)))
    {
        // Both keys point to the same file. Assert that it is a valid short name.
        ASSERT(GetFileName((*Key)->NextKey)->NameLength <= MAX_SHORTNAME_LENGTH);

        // Move to next key
        *Key = (*Key)->NextKey;
        FileNameData = GetFileName(*Key);

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

static
NTSTATUS
ResumeFileBothDirInfoScan(_In_    BOOLEAN ReturnSingleEntry,
                          _Inout_ PBTreeContext BTreeCtx,
                          _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                          _Inout_ PULONG BufferLength)
{
    NTSTATUS Status;
    ULONG EntrySize;

    EntrySize = 0;

    if (!BTreeCtx->CurrentKey ||
        IsEndOfNode(BTreeCtx->CurrentKey))
    {
        // We reached the end of the directory listing.
        return STATUS_NO_MORE_FILES;
    }

    while (BTreeCtx->CurrentKey)
    {
        DumpBTreeKey(BTreeCtx->CurrentKey, 0);

        if (!IsLastEntry(BTreeCtx->CurrentKey))
        {
            // Add key to buffer
            Status = AddKeyToBothDirInfo(&BTreeCtx->CurrentKey,
                                         FALSE,
                                         Buffer,
                                         BufferLength,
                                         &EntrySize);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("We filled the buffer or something.\n");
                return STATUS_SUCCESS;
            }

            if (ReturnSingleEntry)
            {
                Buffer->NextEntryOffset = 0;
                BTreeCtx->CurrentKey = GetNextKey(BTreeCtx->CurrentKey);
                break;
            }

            // Adjust buffer
            Buffer = (PFILE_BOTH_DIR_INFORMATION)((ULONG_PTR)Buffer + EntrySize);
        }

        BTreeCtx->CurrentKey = GetNextKey(BTreeCtx->CurrentKey);

        if (!BTreeCtx->CurrentKey)
        {
            DPRINT1("Terminating last entry!\n");
            // Go back to previous entry and end it
            Buffer = (PFILE_BOTH_DIR_INFORMATION)((ULONG_PTR)Buffer - EntrySize);
            Buffer->NextEntryOffset = 0;
            break;
        }
    }

    return STATUS_SUCCESS;

}

static
NTSTATUS
GetFileBothDirectoryInformation(_In_    PFileContextBlock FileCB,
                                _In_    PVolumeContextBlock VolCB,
                                _In_    UCHAR IrpFlags,
                                _In_    PUNICODE_STRING FileName,
                                _Out_   PFILE_BOTH_DIR_INFORMATION Buffer,
                                _Inout_ PULONG Length)
{
    NTSTATUS Status;
    PBTreeContext BTreeCtx;
    BOOLEAN ReturnSingleEntry, RestartScan;

    ReturnSingleEntry = !!(IrpFlags & SL_RETURN_SINGLE_ENTRY);
    RestartScan = !!(IrpFlags & SL_RESTART_SCAN);
    BTreeCtx = &(FileCB->QueryDirectoryCtx);

    DPRINT1("Length: %ld\n", *Length);

    ASSERT(FileCB);

    if (!BTreeCtx->RootNode)
    {
        DPRINT1("This is not a directory!\n");
        return STATUS_NOT_FOUND;
    }

    if (RestartScan)
    {
        // Reset file both directory context.
        BTreeCtx->CurrentKey = BTreeCtx->RootNode->FirstKey;
        DPRINT1("Restart scan set!\n");
    }

    // TODO: If not root directory, also return . and .. directories maybe?
    ASSERT(Buffer);

    // Clear buffer
    RtlZeroMemory(Buffer, *Length);

    if (FileName)
    {
        // Get the key requested
        BTreeCtx->CurrentKey = FindKeyInNode(BTreeCtx->RootNode,
                                             FileName->Buffer,
                                             FileName->Length);
    }

    // Populate the buffer
    Status = ResumeFileBothDirInfoScan(ReturnSingleEntry,
                                       BTreeCtx,
                                       Buffer,
                                       Length);

    return Status;
}