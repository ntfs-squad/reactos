#pragma once

#include "ntfsprocs.h"
#include <pshpack1.h>
#include <poppack.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif //MAX_PATH

class NtfsPartition;
class FileRecord;

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

typedef struct
{
    ERESOURCE DirResource; //DDK

    KSPIN_LOCK FileCBListLock; //DDK
    LIST_ENTRY FileCBListHead; //WinSDK

    PVPB VolPB; //DDK
    PDEVICE_OBJECT StorageDevice; //DDK
    PFILE_OBJECT StreamFileObject; //DDK

    struct _FCB *RootFileCB;

    NtfsPartition *PartitionObj;

    // We will uncomment this if needed.
    // ULONG Flags;

} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _FCB
{
    FSRTL_COMMON_FCB_HEADER RFCB; // DDK
    SECTION_OBJECT_POINTERS SectionObjectPointers; //DDK

    PFILE_OBJECT FileObject; //DDK
    PVolumeContextBlock VolCB;

    ERESOURCE PagingIoResource; //DDK
    ERESOURCE MainResource; //DDK

    LIST_ENTRY FileCBListEntry; //DDK
    struct _FCB* ParentFileCB;

    WCHAR Stream[MAX_PATH];
    WCHAR *ObjectName;		   // Point on filename (250 chars max) in PathName */
    WCHAR PathName[MAX_PATH];  // Path+Filename 260 max

    FileRecord* FileRec;

    // We will uncomment these when/if we need them.
    // ULONG DirIndex;
    // LONG RefCount;
    // ULONG Flags;
    // ULONG OpenHandleCount;
    // ULONGLONG MFTIndex;
    // USHORT LinkCount;
} FileContextBlock, *PFileContextBlock;
