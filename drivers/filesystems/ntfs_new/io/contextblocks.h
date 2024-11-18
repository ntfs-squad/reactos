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
} VolumeContextBlock, *PVolumeContextBlock;

typedef struct _SCB
{
    SECTION_OBJECT_POINTERS SectionObjectPointers;
} StreamContextBlock, *PStreamContextBlock;

typedef struct _FCB
{
    WCHAR Stream[MAX_PATH];
    WCHAR *ObjectName;		   // Point on filename (250 chars max) in PathName */
    FileRecord* FileRec;

    // NOTE: The members below may or may not get included in file record.

    // Used for file name information;
    WCHAR FileName[MAX_PATH];

    // Used for Alternate Data Streams (ADS)
    AttributeType RequestedType;
    PWSTR RequestedStream;

    // TODO: Where/how is this determined?
    BOOLEAN DeletePending;

    // Used for query directory requests
    class Directory* FileDir;

    // Consider moving, multiple files can point to the same stream in NTFS.
    PStreamContextBlock StreamCB;

} FileContextBlock, *PFileContextBlock;
