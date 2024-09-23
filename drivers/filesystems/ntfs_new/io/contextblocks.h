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
    PIO_STACK_LOCATION Stack;
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    PIRP Irp;
    PDEVICE_OBJECT DeviceObject;

    // We will uncomment these when needed.
    // PFILE_OBJECT FileObject;
    // ULONG Flags;
    // BOOLEAN IsTopLevel;
    // CCHAR PriorityBoost;

} IoRequestContext, *PIoRequestContext;

typedef struct
{
    NtfsPartition *PartitionObj;

    // Not sure how these work yet...
    ERESOURCE DirResource; //DDK
    KSPIN_LOCK FileCBListLock; //DDK
    LIST_ENTRY FileCBListHead; //WinSDK
    PDEVICE_OBJECT StorageDevice; //DDK
    PFILE_OBJECT StreamFileObject; //DDK
    struct _FCB *RootFileCB;

    // We will uncomment these when needed.
    // PVPB VolPB; //DDK
    // ULONG Flags;

} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _FCB
{
    ULONGLONG FileRecordNumber;
    PVolumeContextBlock VolCB;

    WCHAR Stream[MAX_PATH];
    WCHAR *ObjectName;		   // Point on filename (250 chars max) in PathName */
    WCHAR PathName[MAX_PATH];  // Path+Filename 260 max
    FileRecord* FileRec;

    // I'm not sure how these work yet...
    FSRTL_COMMON_FCB_HEADER RFCB; // DDK
    SECTION_OBJECT_POINTERS SectionObjectPointers; //DDK
    PFILE_OBJECT FileObject; //DDK
    ERESOURCE MainResource; //DDK
    struct _FCB* ParentFileCB;

    // We will uncomment these when/if we need them.
    // ERESOURCE PagingIoResource; //DDK
    // LIST_ENTRY FileCBListEntry; //DDK
    // ULONG DirIndex;
    // LONG RefCount;
    // ULONG Flags;
    // ULONG OpenHandleCount;
    // USHORT LinkCount;
} FileContextBlock, *PFileContextBlock;
