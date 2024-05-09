/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file creation APIs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdCreate)
#endif

/* FUNCTIONS ****************************************************************/
extern PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;

_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCreate(_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp)
{
    if (VolumeDeviceObject == NtfsDiskFileSystemDeviceObject)
    {
        /* DeviceObject represents FileSystem instead of logical volume */
        DPRINT("Opening file system\n");
        Irp->IoStatus.Information = FILE_OPENED;
        return STATUS_SUCCESS;
    }
    return STATUS_SUCCESS;
}