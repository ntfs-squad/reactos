/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#pragma once

#include "ntfspch.h"
#include <pshpack1.h>
#include <poppack.h>

#define GetDisposition(x) ((x >> 24) & 0xFF)
#define GetCreateOptions(x) (x & 0xFFFFFF)

class NTFSVolume;
class FileRecord;

typedef struct
{
    NTFSVolume *Volume;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;
} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _SCB
{
    SECTION_OBJECT_POINTERS SectionObjectPointers;
} StreamContextBlock, *PStreamContextBlock;

typedef struct _FCB
{
    FileRecord* FileRec;
    ULONG CreateOptions;
    ACCESS_MASK DesiredAccess;

    // Used for file name information;
    UNICODE_STRING FileName;

    // Used for Alternate Data Streams (ADS)
    AttributeType RequestedType;
    PWSTR RequestedStream;

    // Used for query directory requests
    class Directory* FileDir;

    // Consider moving, multiple files can point to the same stream in NTFS.
    PStreamContextBlock StreamCB;

    // NT Required FCB Header - needed for proper resource management
    FSRTL_COMMON_FCB_HEADER CommonFCBHeader;
    SECTION_OBJECT_POINTERS SectionObjectPointers;
    FILE_LOCK FileLock;
    ERESOURCE MainResource;
    ERESOURCE PagingIoResource;

} FileContextBlock, *PFileContextBlock;
