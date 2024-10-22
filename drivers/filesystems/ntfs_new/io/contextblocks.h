#pragma once

#include "ntfsprocs.h"
#include <pshpack1.h>
#include <poppack.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif //MAX_PATH

class NTFSVolume;
class FileRecord;

typedef struct
{
    NTFSVolume *Volume;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;

    // We will uncomment these when needed.
    // ERESOURCE DirResource; //DDK
    // KSPIN_LOCK FileCBListLock; //DDK
    // LIST_ENTRY FileCBListHead; //WinSDK
    // struct _FCB *RootFileCB;
    // ULONG Flags;

} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _SCB
{
    SECTION_OBJECT_POINTERS SectionObjectPointers;
} StreamContextBlock, *PStreamContextBlock;

// Putting this here is a hack, clean up header files
// TODO: CLEAN UP HEADER FILES!!!!


struct _BTreeFilenameNode;
typedef struct _BTreeFilenameNode BTreeFilenameNode;

// Keys are arranged in nodes as an ordered, linked list
typedef struct _BTreeKey
{
    struct _BTreeKey  *NextKey;
    struct _BTreeKey  *ParentKey;    // Key of the node whose child is this key
    BTreeFilenameNode *LesserChild;  // Child-Node. All the keys in this node will be sorted before IndexEntry
    PIndexEntry       IndexEntry;    // must be last member for FIELD_OFFSET
}BTreeKey, *PBTreeKey;

// Every Node is just an ordered list of keys.
// Sub-nodes can be found attached to a key (if they exist).
// A key's sub-node precedes that key in the ordered list.
typedef struct _BTreeFilenameNode
{
    ULONG KeyCount;
    BOOLEAN HasValidVCN;
    BOOLEAN DiskNeedsUpdating;
    ULONGLONG VCN;
    PBTreeKey FirstKey;
} BTreeFilenameNode, *PBTreeFilenameNode;

typedef struct
{
    PBTreeFilenameNode RootNode;
} BTree, *PBTree;

typedef struct
{
    PBTree    Tree;
    PBTreeKey CurrentKey;
} BTreeContext, *PBTreeContext;

typedef struct _FCB
{
    ULONGLONG FileRecordNumber;

    WCHAR Stream[MAX_PATH];
    WCHAR *ObjectName;		   // Point on filename (250 chars max) in PathName */
    FileRecord* FileRec;

    // NOTE: The members below may or may not get included in file record.

    // Used for file name information;
    WCHAR FileName[MAX_PATH];

    // Used for File Basic Information
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;

    // Used for File Standard Information
    BOOLEAN IsDirectory;
    BOOLEAN DeletePending;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG NumberOfLinks;

    // Used for query directory requests
    BTreeContext QueryDirectoryCtx;

    // Consider moving, multiple files can point to the same stream in NTFS.
    PStreamContextBlock StreamCB;

    // We will uncomment these when/if we need them.
    // PVolumeContextBlock VolCB;
    // FSRTL_COMMON_FCB_HEADER RFCB; // DDK
    // PFILE_OBJECT FileObject; //DDK
    // ERESOURCE MainResource; //DDK
    // struct _FCB* ParentFileCB;
    // ERESOURCE PagingIoResource; //DDK
    // LIST_ENTRY FileCBListEntry; //DDK
    // ULONG DirIndex;
    // LONG RefCount;
    // ULONG Flags;
    // ULONG OpenHandleCount;
    // USHORT LinkCount;
} FileContextBlock, *PFileContextBlock;
