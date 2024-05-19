#pragma once

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
} NTFS_IRP_CONTEXT, *PNTFS_IRP_CONTEXT;

#include <pshpack1.h>
#include <poppack.h>

typedef struct
{
    ULONGLONG DirectoryFileReferenceNumber;
    ULONGLONG CreationTime;
    ULONGLONG ChangeTime;
    ULONGLONG LastWriteTime;
    ULONGLONG LastAccessTime;
    ULONGLONG AllocatedSize;
    ULONGLONG DataSize;
    ULONG FileAttributes;
    union
    {
        struct
        {
            USHORT PackedEaSize;
            USHORT AlignmentOrReserved;
        } EaInfo;
        ULONG ReparseTag;
    } Extended;
    UCHAR NameLength;
    UCHAR NameType;
    WCHAR Name[1];
} FILENAME_ATTRIBUTE, *PFILENAME_ATTRIBUTE;


#ifndef MAX_PATH
#define MAX_PATH   260
#endif //MAX_PATH

typedef struct
{

    ERESOURCE DirResource;
//    ERESOURCE FatResource;

    KSPIN_LOCK FcbListLock;
    LIST_ENTRY FcbListHead;

    PVPB Vpb;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;

    struct _NTFS_ATTR_CONTEXT* MFTContext;
    struct _FILE_RECORD_HEADER* MasterFileTable;
    struct _FCB *VolumeFcb;

    NPAGED_LOOKASIDE_LIST FileRecLookasideList;

    ULONG MftDataOffset;
    ULONG Flags;
    ULONG OpenHandleCount;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION, NTFS_VCB, *PNTFS_VCB;

typedef struct _FCB
{
    //NTFSIDENTIFIER Identifier;

    FSRTL_COMMON_FCB_HEADER RFCB;
    SECTION_OBJECT_POINTERS SectionObjectPointers;

    PFILE_OBJECT FileObject;
    PNTFS_VCB Vcb;

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

    FILENAME_ATTRIBUTE Entry;

} NTFS_FCB, *PNTFS_FCB;
