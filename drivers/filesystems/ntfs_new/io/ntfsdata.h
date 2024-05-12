#pragma once

typedef enum
{
    AttributeStandardInformation = 0x10,
    AttributeAttributeList = 0x20,
    AttributeFileName = 0x30,
    AttributeObjectId = 0x40,
    AttributeSecurityDescriptor = 0x50,
    AttributeVolumeName = 0x60,
    AttributeVolumeInformation = 0x70,
    AttributeData = 0x80,
    AttributeIndexRoot = 0x90,
    AttributeIndexAllocation = 0xA0,
    AttributeBitmap = 0xB0,
    AttributeReparsePoint = 0xC0,
    AttributeEAInformation = 0xD0,
    AttributeEA = 0xE0,
    AttributePropertySet = 0xF0,
    AttributeLoggedUtilityStream = 0x100,
    AttributeEnd = 0xFFFFFFFF
} ATTRIBUTE_TYPE, *PATTRIBUTE_TYPE;

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
typedef struct _BIOS_PARAMETERS_BLOCK
{
    USHORT    BytesPerSector;			// 0x0B
    UCHAR     SectorsPerCluster;		// 0x0D
    UCHAR     Unused0[7];				// 0x0E, checked when volume is mounted
    UCHAR     MediaId;				// 0x15
    UCHAR     Unused1[2];				// 0x16
    USHORT    SectorsPerTrack;		// 0x18
    USHORT    Heads;					// 0x1A
    UCHAR     Unused2[4];				// 0x1C
    UCHAR     Unused3[4];				// 0x20, checked when volume is mounted
} BIOS_PARAMETERS_BLOCK, *PBIOS_PARAMETERS_BLOCK;

typedef struct _EXTENDED_BIOS_PARAMETERS_BLOCK
{
    USHORT    Unknown[2];				// 0x24, always 80 00 80 00
    ULONGLONG SectorCount;			// 0x28
    ULONGLONG MftLocation;			// 0x30
    ULONGLONG MftMirrLocation;		// 0x38
    CHAR      ClustersPerMftRecord;	// 0x40
    UCHAR     Unused4[3];				// 0x41
    CHAR      ClustersPerIndexRecord; // 0x44
    UCHAR     Unused5[3];				// 0x45
    ULONGLONG SerialNumber;			// 0x48
    UCHAR     Checksum[4];			// 0x50
} EXTENDED_BIOS_PARAMETERS_BLOCK, *PEXTENDED_BIOS_PARAMETERS_BLOCK;

typedef struct _BOOT_SECTOR
{
    UCHAR     Jump[3];				// 0x00
    UCHAR     OEMID[8];				// 0x03
    BIOS_PARAMETERS_BLOCK BPB;
    EXTENDED_BIOS_PARAMETERS_BLOCK EBPB;
    UCHAR     BootStrap[426];			// 0x54
    USHORT    EndSector;				// 0x1FE
} BOOT_SECTOR, *PBOOT_SECTOR;
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

typedef struct
{
    ULONG        Type;
    ULONG        Length;
    UCHAR        IsNonResident;
    UCHAR        NameLength;
    USHORT        NameOffset;
    USHORT        Flags;
    USHORT        Instance;
    union
    {
        // Resident attributes
        struct
        {
            ULONG        ValueLength;
            USHORT        ValueOffset;
            UCHAR        Flags;
            UCHAR        Reserved;
        } Resident;
        // Non-resident attributes
        struct
        {
            ULONGLONG        LowestVCN;
            ULONGLONG        HighestVCN;
            USHORT        MappingPairsOffset;
            USHORT        CompressionUnit;
            UCHAR        Reserved[4];
            LONGLONG        AllocatedSize;
            LONGLONG        DataSize;
            LONGLONG        InitializedSize;
            LONGLONG        CompressedSize;
        } NonResident;
    };
} NTFS_ATTR_RECORD, *PNTFS_ATTR_RECORD;

typedef struct _NTFS_ATTR_CONTEXT
{
    PUCHAR            CacheRun;
    ULONGLONG            CacheRunOffset;
    LONGLONG            CacheRunStartLCN;
    ULONGLONG            CacheRunLength;
    LONGLONG            CacheRunLastLCN;
    ULONGLONG            CacheRunCurrentOffset;
    LARGE_MCB           DataRunsMCB;
    ULONGLONG           FileMFTIndex;
    ULONGLONG           FileOwnerMFTIndex; /* If attribute list attribute, reference the original file */
    PNTFS_ATTR_RECORD    pRecord;
} NTFS_ATTR_CONTEXT, *PNTFS_ATTR_CONTEXT;