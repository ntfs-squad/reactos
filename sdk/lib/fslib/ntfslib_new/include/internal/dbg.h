#ifdef NTFS_DEBUG

#define PrintFlag(Item, Flag, FlagName) if(Item & Flag) \
DbgPrint("    %s\n", FlagName); \
/* Debug print functions. REMOVE WHEN DONE. */

static inline void PrintUpCaseTable(PUCHAR UpCaseData,
                                    ULONG Length)
{
    DbgPrint("Offset | Value\n");
    for (int i = 0; i < Length; i += 2)
    {
        DbgPrint("0x%2X   | %C\n", i, ((WCHAR)(UpCaseData[i])));
    }
}

static inline void PrintAttrDefTable(PFileRecord AttrDef)
{
    PAttrDefEntry TableEntry;
    ULONG AttrDefEntryIndex, AttrDefDataSize, MaxIndex;
    PUCHAR Buffer;
    PAttribute DataAttr;

    DataAttr = AttrDef->GetAttribute(TypeData, NULL);
    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDef->CopyData(DataAttr,
                      Buffer,
                      &AttrDefDataSize,
                      0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    AttrDefEntryIndex = 0;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;

    DbgPrint(" Type  | Name                       | Flags | Min  | Max  \n");
    DbgPrint("==========================================================\n");

    for (int i = 0; i < MaxIndex; i++)
    {
        DbgPrint(" 0x%03X | %-26S | 0x%02X  | 0x%02X | 0x%X\n",
                 TableEntry->AttributeType,
                 TableEntry->Label,
                 TableEntry->Flags,
                 TableEntry->MinimumSize,
                 TableEntry->MaximumSize);

        // Move onto the next element
        TableEntry++;
    }
    delete Buffer;
}

static inline void PrintFileRecordHeader(FileRecordHeader* FRH)
{
    DbgPrint("MFT Record Number: %ld\n", FRH->MFTRecordNumber);
}

static inline void PrintAttributeHeader(PAttribute Attr)
{
    DbgPrint("Attribute Type:        0x%X\n", Attr->AttributeType);
    DbgPrint("Length:                %ld\n", Attr->Length);
    DbgPrint("Nonresident Flag:      %ld\n", Attr->IsNonResident);
    DbgPrint("Name Length:           %ld\n", Attr->NameLength);
    DbgPrint("Name Offset:           %ld\n", Attr->NameOffset);
    DbgPrint("Flags:                 0x%X\n", Attr->Flags);
    DbgPrint("Attribute ID:          %ld\n", Attr->AttributeID);

    if (!(Attr->IsNonResident))
    {
        DbgPrint("Data Length:           %ld\n", Attr->Resident.DataLength);
        DbgPrint("Data Offset:           0x%X\n", Attr->Resident.DataLength);
        DbgPrint("Indexed Flag:          %ld\n", Attr->Resident.IndexedFlag);
    }

    else
    {
        DbgPrint("First VCN:             %ld\n", Attr->NonResident.FirstVCN);
        DbgPrint("Last VCN:              %ld\n", Attr->NonResident.LastVCN);
        DbgPrint("Data Run Offset:       %ld\n", Attr->NonResident.DataRunsOffset);
        DbgPrint("Compression Unit Size: %ld\n", Attr->NonResident.CompressionUnitSize);
        DbgPrint("Allocated Size:        %ld\n", Attr->NonResident.AllocatedSize);
        DbgPrint("Data Size:             %ld\n", Attr->NonResident.DataSize);
        DbgPrint("Initialized Data Size: %ld\n", Attr->NonResident.InitalizedDataSize);
    }
}

static inline void PrintFilenameAttrHeader(FileNameEx* Attr)
{
    UINT64 FRN = GetFRNFromFileRef(Attr->ParentFileReference);
    UINT16 SQN = GetSQNFromFileRef(Attr->ParentFileReference);

    DbgPrint("Parent Dir FRN:   %ld\n", FRN);
    DbgPrint("Parent Dir SQN:   %ld\n", SQN);
    DbgPrint("Creation Time:    %ld\n", Attr->CreationTime);
    DbgPrint("Last Write Time:  %ld\n", Attr->LastWriteTime);
    DbgPrint("Change Time:      %ld\n", Attr->ChangeTime);
    DbgPrint("Last Access Time: %ld\n", Attr->LastAccessTime);
    DbgPrint("Allocated Size:   %ld\n", Attr->AllocatedSize);
    DbgPrint("Data Size:        %ld\n", Attr->DataSize);
    DbgPrint("Flags:            0x%X\n", Attr->Flags);
    DbgPrint("Filename:        \"%S\"\n", Attr->Name);
}

static inline void PrintNTFSBootSector(PBootSector PartBootSector)
{
    DbgPrint("OEM ID            %s\n", PartBootSector->OEM_ID);
    DbgPrint("Bytes per sector  %ld\n", PartBootSector->BytesPerSector);
    DbgPrint("Sectors/cluster   %ld\n", PartBootSector->SectorsPerCluster);
    DbgPrint("Sectors per track %ld\n", PartBootSector->SectorsPerTrack);
    DbgPrint("Number of heads   %ld\n", PartBootSector->NumberOfHeads);
    DbgPrint("Sectors in volume %ld\n", PartBootSector->SectorsInVolume);
    DbgPrint("LCN for $MFT      %ld\n", PartBootSector->MFTLCN);
    DbgPrint("LCN for $MFT_MIRR %ld\n", PartBootSector->MFTMirrLCN);
    DbgPrint("Clusters/MFT Rec  %d\n", PartBootSector->ClustersPerFileRecord);
    DbgPrint("Clusters/IndexRec %d\n", PartBootSector->ClustersPerIndexRecord);
    DbgPrint("Serial number     0x%X\n", PartBootSector->SerialNumber);
};

static inline void PrintStdInfoEx(StandardInformationEx* StdInfo)
{
    DbgPrint("Change Time:      %lu\n", StdInfo->ChangeTime);
    DbgPrint("Last Access Time: %lu\n", StdInfo->LastAccessTime);
    DbgPrint("Last Write Time:  %lu\n", StdInfo->LastWriteTime);
    DbgPrint("Creation Time:    %lu\n", StdInfo->CreationTime);
}

static inline void PrintIndexRootEx(PIndexRootEx IndexRootData)
{
    DbgPrint("Attribute Type:            0x%X\n", IndexRootData->AttributeType);
    DbgPrint("Collation Rule:            0x%X\n", IndexRootData->CollationRule);
    DbgPrint("Bytes per Index Record:    %ld\n", IndexRootData->BytesPerIndexRec);
    DbgPrint("Clusters per Index Record: %ld\n", IndexRootData->ClusPerIndexRec);
}

static inline void PrintFileBothDirEntry(PFILE_BOTH_DIR_INFORMATION Data)
{
    DbgPrint("Short Name:        \"%S\"\n", Data->ShortName);
    DbgPrint("Short Name Length: %ld\n", Data->ShortNameLength);
    DbgPrint("File Name:         \"%S\"\n", Data->FileName);
    DbgPrint("File Name Length:  %ld\n", Data->FileNameLength);
}

static inline void PrintFileBothDirInfo(PFILE_BOTH_DIR_INFORMATION Info, UINT Depth)
{
    PFILE_BOTH_DIR_INFORMATION CurrentStruct = Info;

    for (int i = 0; i < Depth; i++)
    {
        if (CurrentStruct)
        {
            PrintFileBothDirEntry(CurrentStruct);
            if (CurrentStruct->NextEntryOffset)
                CurrentStruct = (PFILE_BOTH_DIR_INFORMATION)((char*)CurrentStruct + CurrentStruct->NextEntryOffset);
            else
                CurrentStruct = NULL;
        }
    }
}

static inline void PrintFileCreateOptions(UINT8 Disposition, ULONG CreateOptions)
{
    switch (Disposition)
    {
        case FILE_SUPERSEDE:
            DbgPrint("Disposition: FILE_SUPERSEDE\n");
            break;
        case FILE_CREATE:
            DbgPrint("Disposition: FILE_CREATE\n");
            break;
        case FILE_OPEN:
            DbgPrint("Disposition: FILE_OPEN\n");
            break;
        case FILE_OPEN_IF:
            DbgPrint("Disposition: FILE_OPEN_IF\n");
            break;
        case FILE_OVERWRITE:
            DbgPrint("Disposition: FILE_OVERWRITE\n");
            break;
        case FILE_OVERWRITE_IF:
            DbgPrint("Disposition: FILE_OVERWRITE_IF\n");
            break;
        default:
            DbgPrint("Disposition: UNKNOWN\n");
            break;
    }

    DbgPrint("Create Options Flags:\n");
    PrintFlag(CreateOptions, FILE_DIRECTORY_FILE, "FILE_DIRECTORY_FILE");
    PrintFlag(CreateOptions, FILE_NON_DIRECTORY_FILE, "FILE_NON_DIRECTORY_FILE");
    PrintFlag(CreateOptions, FILE_WRITE_THROUGH, "FILE_WRITE_THROUGH");
    PrintFlag(CreateOptions, FILE_SEQUENTIAL_ONLY, "FILE_SEQUENTIAL_ONLY");
    PrintFlag(CreateOptions, FILE_RANDOM_ACCESS, "FILE_RANDOM_ACCESS");
    PrintFlag(CreateOptions, FILE_NO_INTERMEDIATE_BUFFERING, "FILE_NO_INTERMEDIATE_BUFFERING");
    PrintFlag(CreateOptions, FILE_SYNCHRONOUS_IO_ALERT, "FILE_SYNCHRONOUS_IO_ALERT");
    PrintFlag(CreateOptions, FILE_SYNCHRONOUS_IO_NONALERT, "FILE_SYNCHRONOUS_IO_NONALERT");
    PrintFlag(CreateOptions, FILE_CREATE_TREE_CONNECTION, "FILE_CREATE_TREE_CONNECTION");
    PrintFlag(CreateOptions, FILE_COMPLETE_IF_OPLOCKED, "FILE_COMPLETE_IF_OPLOCKED");
    PrintFlag(CreateOptions, FILE_NO_EA_KNOWLEDGE, "FILE_NO_EA_KNOWLEDGE");
    PrintFlag(CreateOptions, FILE_OPEN_REPARSE_POINT, "FILE_OPEN_REPARSE_POINT");
    PrintFlag(CreateOptions, FILE_DELETE_ON_CLOSE, "FILE_DELETE_ON_CLOSE");
    PrintFlag(CreateOptions, FILE_OPEN_BY_FILE_ID, "FILE_OPEN_BY_FILE_ID");
    PrintFlag(CreateOptions, FILE_OPEN_FOR_BACKUP_INTENT, "FILE_OPEN_FOR_BACKUP_INTENT");
    // PrintFlag(CreateOptions, FILE_OPEN_REQUIRING_OPLOCK, "FILE_OPEN_REQUIRING_OPLOCK");
    PrintFlag(CreateOptions, FILE_RESERVE_OPFILTER, "FILE_RESERVE_OPFILTER");
}

#endif // NTFS_DEBUG
