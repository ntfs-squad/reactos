/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

NTSTATUS
Directory::VerifyUpdateSequenceArray(PNTFSRecordHeader Record)
{
    const ULONG BytesPerSector = DiskVolume->BytesPerSector;
    const ULONG RecordSize = BytesPerIndexRecord(DiskVolume);
    const ULONG UsaOffset = Record->UpdateSequenceOffset;
    const ULONG UsaCount = Record->SizeOfUpdateSequence;
    const PUSHORT UpdateSequenceArray = (PUSHORT)((PUCHAR)Record + UsaOffset);
    const USHORT UpdateSequenceNumber = UpdateSequenceArray[0];
    PUSHORT SectorEnd;

    /* Check for corruption based on incompatible Usa data. */
    if (UsaCount == 0 ||
        UsaCount - 1 > RecordSize / BytesPerSector ||
        UsaOffset > RecordSize ||
        UsaCount > (RecordSize - UsaOffset) / sizeof(USHORT))
    {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    /* The end of a sector in a file record should always be the
     * Update Sequence Number (USN).
     */
    for (ULONG Index = 1; Index < UsaCount; ++Index)
    {
        SectorEnd = (PUSHORT)((PUCHAR)Record + (Index * BytesPerSector) - sizeof(USHORT));
        if (*SectorEnd != UpdateSequenceNumber)
            return STATUS_FILE_CORRUPT_ERROR;
        *SectorEnd = UpdateSequenceArray[Index];
    }

    return STATUS_SUCCESS;
}

NTSTATUS
Directory::CreateNode(_In_ PFileRecord File,
                      _In_ PAttribute IndexAllocation,
                      _Inout_ PBTreeKey ParentKey)
{
    NTSTATUS Status;
    const ULONG RecordSize = BytesPerIndexRecord(DiskVolume);
    ULONGLONG Vcn;
    RtlCopyMemory(&Vcn,
                  reinterpret_cast<PUCHAR>(ParentKey->Entry) + ParentKey->Entry->EntryLength - sizeof(Vcn),
                  sizeof(Vcn));

    PIndexBuffer Buffer = static_cast<PIndexBuffer>(NtfsAllocatePoolWithTag(NtfsPagedPool, RecordSize, TAG_BTREE));
    if (!Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    ULONG BytesRemaining = RecordSize;
    // TODO: Check the directory bitmap before loading this index record.
    Status = File->CopyData(IndexAllocation,
                            reinterpret_cast<PUCHAR>(Buffer),
                            &BytesRemaining,
                            GetAllocationOffsetFromVCN(Vcn));
    if (!NT_SUCCESS(Status))
        goto Done;
    if (BytesRemaining || RtlCompareMemory(Buffer->RecordHeader.TypeID, "INDX", 4) != 4)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    Status = VerifyUpdateSequenceArray(&Buffer->RecordHeader);
    if (!NT_SUCCESS(Status))
        goto Done;
    if (Buffer->VCN != Vcn)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    ParentKey->ChildNode = new(NtfsPagedPool, TAG_BTREE) BTreeNode();
    if (!ParentKey->ChildNode)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    ParentKey->ChildNode->VCN = Vcn;
    Status = LoadNodeEntries(File, IndexAllocation, &Buffer->IndexHeader,
                             RecordSize - FIELD_OFFSET(IndexBuffer, IndexHeader),
                             ParentKey->ChildNode, ParentKey);

Done:
    NtfsFreePool(Buffer);
    return Status;
}

NTSTATUS
Directory::LoadRootNode(_In_ PFileRecord File,
                        _Inout_ PBTreeNode Node)
{
    PAttribute Root = File->GetAttribute(TypeIndexRoot, NULL);
    if (!Root ||
        Root->IsNonResident ||
        Root->Resident.DataOffset < ATTR_RESIDENT_HEADER_SIZE ||
        Root->Resident.DataOffset > Root->Length ||
        Root->Resident.DataLength > Root->Length - Root->Resident.DataOffset ||
        Root->Resident.DataLength < sizeof(IndexRootEx))
    {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    PIndexRootEx IndexRoot = reinterpret_cast<PIndexRootEx>(GetResidentDataPointer(Root));
    return LoadNodeEntries(File, File->GetAttribute(TypeIndexAllocation, L"$I30"),
                           &IndexRoot->Header,
                           Root->Resident.DataLength - FIELD_OFFSET(IndexRootEx, Header),
                           Node, NULL);
}

NTSTATUS
Directory::LoadNodeEntries(_In_ PFileRecord File,
                           _In_opt_ PAttribute IndexAllocation,
                           _In_ const IndexNodeHeader* Header,
                           _In_ ULONG HeaderBytes,
                           _Inout_ PBTreeNode Node,
                           _In_opt_ PBTreeKey ParentKey)
{
    if (Header->IndexOffset < sizeof(*Header) ||
        Header->IndexOffset > Header->TotalIndexSize ||
        Header->TotalIndexSize > HeaderBytes)
    {
        return STATUS_FILE_CORRUPT_ERROR;
    }

    const UCHAR* Bytes = reinterpret_cast<const UCHAR*>(Header);
    const ULONG EntryHeaderSize = FIELD_OFFSET(IndexEntry, IndexStream);
    PBTreeKey* Next = &Node->FirstKey;
    for (ULONG Offset = Header->IndexOffset; Offset < Header->TotalIndexSize;)
    {
        const ULONG Remaining = Header->TotalIndexSize - Offset;
        if (Remaining < EntryHeaderSize)
            return STATUS_FILE_CORRUPT_ERROR;

        const IndexEntry* Entry = reinterpret_cast<const IndexEntry*>(Bytes + Offset);
        const BOOL HasChild = (Entry->Flags & INDEX_ENTRY_NODE) != 0;
        const ULONG Overhead = EntryHeaderSize + (HasChild ? sizeof(ULONGLONG) : 0);
        if (Entry->EntryLength < Overhead ||
            Entry->EntryLength > Remaining ||
            Entry->StreamLength > Entry->EntryLength - Overhead ||
            (HasChild && !IndexAllocation))
        {
            return STATUS_FILE_CORRUPT_ERROR;
        }

        if (!(Entry->Flags & INDEX_ENTRY_END))
        {
            const ULONG NameOffset = FIELD_OFFSET(FileNameEx, Name);
            if (Entry->StreamLength < NameOffset)
                return STATUS_FILE_CORRUPT_ERROR;

            const FileNameEx* Name = reinterpret_cast<const FileNameEx*>(Entry->IndexStream);
            if (Name->NameLength * sizeof(WCHAR) > Entry->StreamLength - NameOffset)
                return STATUS_FILE_CORRUPT_ERROR;
        }

        PBTreeKey Key = new(NtfsPagedPool, TAG_BTREE) BTreeKey();
        if (!Key)
            return STATUS_INSUFFICIENT_RESOURCES;
        *Next = Key;
        Next = &Key->NextKey;
        Key->ParentNodeKey = ParentKey;
        Key->Entry = static_cast<PIndexEntry>(NtfsAllocatePoolWithTag(
            ParentKey ? NtfsPagedPool : NtfsNonPagedPool, Entry->EntryLength, TAG_BTREE));
        if (!Key->Entry)
            return STATUS_INSUFFICIENT_RESOURCES;
        RtlCopyMemory(Key->Entry, Entry, Entry->EntryLength);

        if (HasChild)
        {
            NTSTATUS Status = CreateNode(File, IndexAllocation, Key);
            if (!NT_SUCCESS(Status))
                return Status;
        }

        if (Key->IsLastEntry())
            return STATUS_SUCCESS;

        Offset += Entry->EntryLength;
    }

    return STATUS_FILE_CORRUPT_ERROR;
}

NTSTATUS
Directory::LoadDirectory(_In_ PFileRecord File,
                         _In_ BOOL SkipShortNameMarking)
{
    if (!FileRecordIsDirectory(File))
        return STATUS_NOT_A_DIRECTORY;

    BTreeNode NewRoot;
    NTSTATUS Status = LoadRootNode(File, &NewRoot);
    if (!NT_SUCCESS(Status))
        return Status;

    // Update keys
    PBTreeKey OldFirstKey = RootNode.FirstKey;
    RootNode.FirstKey = NewRoot.FirstKey;
    NewRoot.FirstKey = OldFirstKey;
    CurrentKey = RootNode.FirstKey;

    // Optimization: Skip marking short name aliases if requested.
    // TODO: Also skip marking if short names are disabled for this volume
    if (SkipShortNameMarking)
       return STATUS_SUCCESS;

    // Mark short-name aliases so enumeration can skip them.
    for (PBTreeKey Key = CurrentKey; Key; Key = Key->NextInTree())
    {
        if (!Key->IsLastEntry() && !(Key->Flags & DIR_KEY_8DOT3))
        {
            PBTreeKey ShortName = GetShortNameKey(Key, FALSE);
            if (ShortName)
                ShortName->Flags |= DIR_KEY_8DOT3;
        }
    }

    return STATUS_SUCCESS;
}

BOOLEAN
Directory::DoesFileNameMatch(PUNICODE_STRING NameFilter,
                             PBTreeKey Key,
                             BOOLEAN IgnoreCase)
{
    UNICODE_STRING FileNameString;
    PFileNameEx FileNameData;

    // Optimization: last key is a dummy key and won't match with any file.
    if (Key->IsLastEntry())
        return FALSE;

    FileNameData = GetFileName(Key);
    RtlInitEmptyUnicodeString(&FileNameString,
                              FileNameData->Name,
                              ((FileNameData->NameLength) * sizeof(WCHAR)));
    FileNameString.Length = FileNameString.MaximumLength;

    if (IgnoreCase)
    {
        /* Note: if we want to do case-insensitive searching, we must make
         * NameFilter all uppercase.
         *
         * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-_fsrtl_advanced_fcb_header-fsrtlisnameinexpression
        */
        NTSTATUS Status = DiskVolume->UpcaseUnicodeString(NameFilter);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to upcase name filter! Performing case sensitive matching...\n");
            IgnoreCase = FALSE;
        }
    }

    return NtfsIsNameInExpression(NameFilter,
                                  &FileNameString,
                                  IgnoreCase,
                                  NULL);
}

PBTreeKey
Directory::GetShortNameKey(_In_ PBTreeKey Key,
                           _In_ BOOLEAN SkipNonShortNames)
{
    /* Search file tree for the next entry with the same file record
     * number (FRN).
     *
     * NOTE: I have seen short file name entries 3+ keys after
     * the long file name entry, but never before the long name entry
     * (see: system32). Most of the time, it's the next key.
     *
     * If you do observe a short file name entry before the long file
     * name entry, please update the search algorithm.
     */

    PBTreeKey FoundKey;
    PFileNameEx FileNameData;
    ULONGLONG TargetFRN;

    // If this is a dummy key, there is no short name
    if (Key->IsLastEntry())
    {
        DPRINT1("Tried to find short name for a dummy key!\n");
        DPRINT1("FIXME: Rework whatever algorithm to prevent this.\n");
        return NULL;
    }

    // If the key is already a legal short name, there isn't another short name
    FileNameData = GetFileName(Key);
    if (IsLegal8Dot3ShortName(FileNameData->Name, FileNameData->NameLength))
        return NULL;

    TargetFRN = GetFRNFromFileRef(FileRef(Key));
    FoundKey = Key->NextInTree();

    /* This algorithm does not go back to search from the beginning of the tree.
     * If you observe a short name bug, this is probably it.
     */
    while (FoundKey)
    {
        if (!FoundKey->IsLastEntry() &&
            (!SkipNonShortNames || (FoundKey->Flags & DIR_KEY_8DOT3)))
        {
            FileNameData = GetFileName(FoundKey);
            if (GetFRNFromFileRef(FileRef(FoundKey)) == TargetFRN
                && IsLegal8Dot3ShortName(FileNameData->Name, FileNameData->NameLength))
            {
                return FoundKey;
            }
        }
        FoundKey = FoundKey->NextInTree();
    }

    /* If we don't find the short name, no short file name was generated.
     * This is not necessarily an error.
     */

    return NULL;
}

Directory::Directory(_In_ PVolume DiskVolume)
{
    this->DiskVolume = DiskVolume;
}
