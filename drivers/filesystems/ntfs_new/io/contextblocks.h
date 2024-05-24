#pragma once

#include <pshpack1.h>
#include <poppack.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif //MAX_PATH

typedef struct
{
    ULONG Flags;
    PIO_STACK_LOCATION Stack;
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    PIRP Irp;
    BOOLEAN IsTopLevel;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject;
    CCHAR PriorityBoost;

} IoRequestContext, *PIoRequestContext;

/* TODO: Remove?
typedef struct
{
    LIST_ENTRY     NextCCB;
    PFILE_OBJECT   PtrFileObject;
    LARGE_INTEGER  CurrentByteOffset;

    // for DirectoryControl
    ULONG Entry;
    PWCHAR DirectorySearchPattern;

    ULONG LastCluster;
    ULONG LastOffset;
} ClusterContextBlock, *PClusterContextBlock;
*/

typedef struct
{
    ERESOURCE DirResource;

    KSPIN_LOCK FileCBListLock;
    LIST_ENTRY FileCBListHead;

    PVPB Vpb;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;

    struct _NTFS_ATTR_CONTEXT* MFTContext;
    struct _FILE_RECORD_HEADER* MasterFileTable;
    struct _FCB *VolumeFileCB;

    NPAGED_LOOKASIDE_LIST FileRecLookasideList;

    ULONG MftDataOffset;
    ULONG Flags;
    ULONG OpenHandleCount;

} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _FCB
{
    //NTFSIDENTIFIER Identifier;

    FSRTL_COMMON_FCB_HEADER RFCB;
    SECTION_OBJECT_POINTERS SectionObjectPointers;

    PFILE_OBJECT FileObject;
    PVolumeContextBlock VolCB;

    WCHAR Stream[MAX_PATH];
    WCHAR *ObjectName;		/* point on filename (250 chars max) in PathName */
    WCHAR PathName[MAX_PATH];	/* path+filename 260 max */

    ERESOURCE PagingIoResource;
    ERESOURCE MainResource;

    LIST_ENTRY FcbListEntry;
    struct _FCB* ParentFcb;

    ULONG DirIndex;

    LONG RefCount;
    ULONG Flags;
    ULONG OpenHandleCount;

    ULONGLONG MFTIndex;
    USHORT LinkCount;

    // FILENAME_ATTRIBUTE Entry;

} FileContextBlock, *PFileContextBlock;
