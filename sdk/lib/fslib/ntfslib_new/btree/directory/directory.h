/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#define GetFileName(Key) \
((PFileNameEx)((Key)->Entry->IndexStream))

#define GetVCN(NodeKey) \
(PULONGLONG)(NodeKey->Entry + NodeKey->Entry->EntryLength - sizeof(ULONGLONG))

// Hack for now so I know what maps to what
#define GetIndexEntryVCN(IndexEntry) *GetVCN(IndexEntry)

// Calculates start of Index Buffer relative to the index allocation, given the node's VCN
#define GetAllocationOffsetFromVCN(VCN) \
(BytesPerIndexRecord(DiskVolume) < BytesPerCluster(DiskVolume)) ? \
(VCN * (DiskVolume->BytesPerSector)) : \
(VCN * BytesPerCluster(DiskVolume))

#define IsLastEntry(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_END)

#define IsIndexNode(Key) !!((Key)->Entry->Flags & INDEX_ENTRY_NODE)

#define IsEndOfNode(Key) IsLastEntry(Key) && !IsIndexNode(Key)

#define GetNextKey(Key) \
IsIndexNode(Key) ? (Key)->ChildNode->FirstKey : IsEndOfNode(Key) ? \
(Key)->ParentNodeKey ? (Key)->ParentNodeKey->NextKey : NULL : (Key)->NextKey

#define IsLowercaseCharacterW(wchar) wchar >= L'a' && wchar <= L'z'

#define IsDotOrDotDotDirW(Buffer, Length) \
Buffer[0] == L'.' \
? Length == sizeof(WCHAR) || (Buffer[1] == L'.' && Length == 2 * sizeof(WCHAR)) \
: FALSE

#define DIR_KEY_8DOT3 1

#ifdef __cplusplus

class Directory : BTree
{
public:
    // ./directory.cpp
    Directory(_In_ PVolume DiskVolume);
    Directory(_In_ PFileRecord File,
              _In_ PVolume DiskVolume);
    NTSTATUS
    LoadDirectory(_In_ PFileRecord File);

    // ./find.cpp
    NTSTATUS
    FindNextFile(_In_  PWCHAR FileName,
                 _Out_ PULONGLONG FileRecordNumber);

    // ./get.cpp
    NTSTATUS
    GetFileBothDirInfo(_In_    BOOLEAN ReturnSingleEntry,
                       _In_    BOOLEAN RestartScan,
                       _In_    PUNICODE_STRING FileNameFilter,
                       _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
                       _Inout_ PULONG BufferLength);

    // ./editdir.cpp
    NTSTATUS
    AddFileToDirectory(_In_ PFileNameEx FileToAdd);

    NTSTATUS
    RemoveFileFromDirectory(_In_ PBTreeKey FileToRemove);

    // ./dbg.cpp
    void
    DumpFileTree();

private:
    PVolume DiskVolume;

    // ./directory.cpp
    NTSTATUS
    VerifyUpdateSequenceArray(PNTFSRecordHeader Record);
    NTSTATUS
    CreateNode(_In_    PFileRecord File,
               _In_    PAttribute  IndexAllocationAttribute,
               _Inout_ PBTreeKey   ParentNodeKey);
    NTSTATUS
    CreateRootNode(_In_  PFileRecord File,
                   _Out_ PBTreeNode *NewRootNode);
    BOOLEAN
    DoesFileNameMatch(PUNICODE_STRING NameFilter,
                      PBTreeKey Key,
                      BOOLEAN IgnoreCase = TRUE);
    PBTreeKey
    GetShortNameKey(_In_ PBTreeKey Key,
                    _In_ BOOLEAN SkipNonShortNames = TRUE);

    // ./find.cpp
    PBTreeKey
    FindKeyInNode(PUNICODE_STRING FileName,
                  PBTreeKey Key);

    // ./get.cpp
    BOOLEAN
    IsEligibleForFileDir(PBTreeKey Key,
                         PUNICODE_STRING FileNameFilter);

    // ./util.cpp
    BOOLEAN
    IsLegalShortNameCharacterW(_In_ WCHAR Char);
    BOOLEAN
    IsLegal8Dot3ShortName(_In_ PWSTR Buffer,
                          _In_ USHORT Length);
};

typedef Directory* PDirectory;

#endif //__cplusplus

typedef struct NtfsDirectory NtfsDirectory;
typedef NtfsDirectory* PNtfsDirectory;

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NTAPI
NtfsDirectoryGetFileBothDirInfo(
    _In_    PNtfsDirectory Dir,
    _In_    BOOLEAN ReturnSingleEntry,
    _In_    BOOLEAN RestartScan,
    _In_    PUNICODE_STRING FileNameFilter,
    _Inout_ PFILE_BOTH_DIR_INFORMATION Buffer,
    _Inout_ PULONG BufferLength);

#ifdef __cplusplus
}
#endif
