/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new ea
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdQueryEa)
#pragma alloc_text(PAGE, NtfsFsdSetEa)
#endif

/* FUNCTIONS ****************************************************************/

_Function_class_(IRP_MJ_QUERY_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryEa(_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp)
{
    DPRINT1("NtfsFsdQueryEa() called\n");
    return 0;
}

_Function_class_(IRP_MJ_SET_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetEa(_In_ PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp)
{
    DPRINT1("NtfsFsdSetEa() called\n");
    return 0;
}