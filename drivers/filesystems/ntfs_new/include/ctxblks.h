/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include <ntifs.h>

// attributes.h needs to be included before this one.
#include <pshpack1.h>
#include <poppack.h>

#define GetDisposition(x) ((x >> 24) & 0xFF)
#define GetCreateOptions(x) (x & 0xFFFFFF)

typedef struct NtfsVolume NtfsVolume;
typedef NtfsVolume* PNtfsVolume;

typedef struct NtfsFileRecord NtfsFileRecord;
typedef NtfsFileRecord* PNtfsFileRecord;

typedef struct NtfsDirectory NtfsDirectory;
typedef NtfsDirectory* PNtfsDirectory;

typedef struct _VolumeContextBlock
{
    PNtfsVolume DiskVolume;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;
} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _SCB
{
    SECTION_OBJECT_POINTERS SectionObjectPointers;
} StreamContextBlock, *PStreamContextBlock;

typedef struct _FCB
{
    PNtfsFileRecord FileRec;
    ULONG CreateOptions;
    ACCESS_MASK DesiredAccess;

    // Used for file name information;
    UNICODE_STRING FileName;

    // Used for Alternate Data Streams (ADS)
    AttributeType RequestedType;
    PWSTR RequestedStream;

    // Used for query directory requests
    PNtfsDirectory FileDir;

    // Consider moving, multiple files can point to the same stream in NTFS.
    PStreamContextBlock StreamCB;

    // NT Required FCB Header - needed for proper resource management
    FSRTL_COMMON_FCB_HEADER CommonFCBHeader;
    SECTION_OBJECT_POINTERS SectionObjectPointers;
    FILE_LOCK FileLock;
    ERESOURCE MainResource;
    ERESOURCE PagingIoResource;
    FAST_MUTEX HeaderMutex;

} FileContextBlock, *PFileContextBlock;
