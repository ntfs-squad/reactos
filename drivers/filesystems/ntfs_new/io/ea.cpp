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
    /* Overview:
     * Reads extended attributes of files.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-query-ea
     */

    // TODO: Implement for real.
    DPRINT1("NtfsFsdQueryEa() called\n");
    return STATUS_EAS_NOT_SUPPORTED;
}

_Function_class_(IRP_MJ_SET_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetEa(_In_ PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp)
{
    /* Overview:
     * Sets extended attributes of files.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-set-ea
     */

    // TODO: Implement for real.
    DPRINT1("NtfsFsdSetEa() called\n");
    return STATUS_EAS_NOT_SUPPORTED;
}