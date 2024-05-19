#pragma once

class FileContextBlock
{
    PFILE_OBJECT FileObject;

    ULONGLONG MFTIndex;
};

class VolumeContextBlock
{
public:
    ULONG  BytesPerSector;
    UINT8  SectorsPerCluster;
    UINT64 SectorsInVolume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    INT32  ClustersPerFileRecord;
    INT32  ClustersPerIndexRecord;
    UINT64 SerialNumber;

    PVPB VolParamBlock;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;

    ULONG Flags;
    ULONG OpenHandleCount;
};

class ClusterContextBlock
{

};

/*typedef struct
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

} DEVICE_EXTENSION, *PDEVICE_EXTENSION, NTFS_VCB, *PNTFS_VCB;*/

/*typedef struct _FCB
{
    //NTFSIDENTIFIER Identifier;

    FSRTL_COMMON_FCB_HEADER RFCB;
    SECTION_OBJECT_POINTERS SectionObjectPointers;

    PFILE_OBJECT FileObject;
    PNTFS_VCB Vcb;

    WCHAR Stream[MAX_PATH];
    WCHAR *ObjectName;		    // point on filename (250 chars max) in PathName
    WCHAR PathName[MAX_PATH];	// path+filename 260 max

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

} NTFS_FCB, *PNTFS_FCB;*/