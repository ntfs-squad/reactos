/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Header file for the ntfs_new procs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#ifndef _NTFSPROCS_
#define _NTFSPROCS_

#include <ntifs.h>

#include <ntddscsi.h>
#include <scsi.h>
#include <ntddcdrm.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include <ntintsafe.h>
#include <pseh/pseh2.h>
#include <ntstrsafe.h>
#include <debug.h>

#define NTFS_DEBUG

#define GetWStrLength(x) x * sizeof(WCHAR)
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))
#define ULONG_ROUND_UP(x)   ROUND_UP((x), (sizeof(ULONG)))
#define MAX_SHORTNAME_LENGTH 12
#define FileRef(Key) (Key)->Entry->Data.Directory.IndexedFile
#define GetUserBuffer(Irp) Irp->MdlAddress ?\
MmGetSystemAddressForMdlSafe(Irp->MdlAddress, ((Irp->Flags & IRP_PAGING_IO) ? HighPagePriority : NormalPagePriority)) :\
Irp->UserBuffer
#define GetBuffer(Irp) Irp->AssociatedIrp.SystemBuffer ? Irp->AssociatedIrp.SystemBuffer : GetUserBuffer(Irp)

typedef enum _TYPE_OF_OPEN {
    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile,
} TYPE_OF_OPEN;

#include "include/tags.h"
#include "include/dispatch.h"
#include "include/attributes.h"
#include "include/reg.h"
#include "include/ctxblks.h"
#ifdef __cplusplus
#include "include/ntfsvol.h"
#endif
#include "include/filerecord.h"
#include "include/btree.h"

#ifndef __cplusplus
extern PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;
#endif

#ifdef __cplusplus

extern "C" {
    extern BOOLEAN gAllowExtChar8dot3;
    extern BOOLEAN gShowMetadataFiles;
    extern BOOLEAN gShowVersionInfo;
    extern BOOLEAN gBugCheckOnCorrupt;
    extern BOOLEAN gDisable8dot3NameCreation;
    extern INT gDisableLastAccessUpdate;
    extern BOOLEAN gDisableLfsDowngrade;
    extern BOOLEAN gDisableLfsUpgrade;
    extern INT gMftZoneReservation;

    extern PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;
    extern PDRIVER_OBJECT NtfsDriverObject;
    extern FAST_IO_DISPATCH FastIoDispatch;
}

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);

#include "include/mft.h"
#include "include/lfs.h"
#include "include/dbg.h"

#endif /* __cplusplus */

#endif // _NTFSPROCS_
