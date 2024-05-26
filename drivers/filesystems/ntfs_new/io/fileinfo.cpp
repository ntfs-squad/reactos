/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file information
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdQueryInformation)
#pragma alloc_text(PAGE, NtfsFsdSetInformation)
#pragma alloc_text(PAGE, NtfsFsdDirectoryControl)
#endif

/* FUNCTIONS ****************************************************************/

_Function_class_(IRP_MJ_QUERY_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp)
{
    __debugbreak();
    return 0;
}

_Function_class_(IRP_MJ_SET_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp)
{
    __debugbreak();
    return 0;
}

_Function_class_(IRP_MJ_DIRECTORY_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDirectoryControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp)
{
    // TODO: make this actually work
    UNREFERENCED_PARAMETER(VolumeDeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    DPRINT1("Called NtfsFsdDirectoryControl() which is a STUB!\n");
    return STATUS_SUCCESS;
}