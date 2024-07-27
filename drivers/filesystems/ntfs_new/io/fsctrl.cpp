/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the filesystem controls
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdFlushBuffers)
#endif

/* FUNCTIONS ****************************************************************/

BOOLEAN
NtfsIsIrpTopLevel (
    IN PIRP Irp
    )
{
    PAGED_CODE();

    if ( IoGetTopLevelIrp() == NULL ) {

        IoSetTopLevelIrp( Irp );

        return TRUE;

    } else {

        return FALSE;
    }
}

/* INCOMPLETE */
_Function_class_(IRP_MJ_FILE_SYSTEM_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFileSystemControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                         _Inout_ PIRP Irp)
{
    /* Overview:
     * Handles FSCTL requests.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-file-system-control
     */
    NTSTATUS Status;
    BOOLEAN TopLevel;

    PAGED_CODE();
    FsRtlEnterFileSystem();
    TopLevel = NtfsIsIrpTopLevel( Irp );
    /* SEH TRY? */
    PIO_STACK_LOCATION IrpSp;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    if ((IrpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
        (IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
        (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_INVALIDATE_VOLUMES))
    {
        Irp->IoStatus.Status = STATUS_UNRECOGNIZED_VOLUME;
        Status = STATUS_UNRECOGNIZED_VOLUME;
        IoCompleteRequest(Irp, NULL);
    }
    else
    {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Status = STATUS_UNRECOGNIZED_VOLUME;

        switch (IrpSp->MinorFunction) {

        case IRP_MN_USER_FS_REQUEST:

            __debugbreak();
            break;

        case IRP_MN_MOUNT_VOLUME:
            Irp->IoStatus.Status = NtfsMountVolume(IrpSp->Parameters.MountVolume.DeviceObject,
                                                   IrpSp->Parameters.MountVolume.Vpb,
                                                   IrpSp->DeviceObject);
            Status = Irp->IoStatus.Status;
            break;
        case IRP_MN_VERIFY_VOLUME:
            __debugbreak();
            break;
        default:
                DPRINT("Invalid FS Control Minor Function %08lx\n", IrpSp->MinorFunction);

                Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
                break;
        }
        Status = Irp->IoStatus.Status;
        IoCompleteRequest(Irp, NULL);
    }
    if (TopLevel) { IoSetTopLevelIrp( NULL ); }
    FsRtlExitFileSystem();

    return Status;
}

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsCommonFileSystemControl(_In_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PAGED_CODE();
    __debugbreak();
    /* Get a pointer to the current Irp stack location */
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    switch (IrpSp->MinorFunction) {

    case IRP_MN_USER_FS_REQUEST:
        __debugbreak();
        break;

    case IRP_MN_MOUNT_VOLUME:
        __debugbreak();
        break;

    case IRP_MN_VERIFY_VOLUME:
        __debugbreak();
        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return Status;
}


_Function_class_(IRP_MJ_FLUSH_BUFFERS)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFlushBuffers(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                    _Inout_ PIRP Irp)
{
    /* Overview:
     * Write all changes from buffer to disk.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-flush-buffers
     */
    __debugbreak();
    return 1;
}