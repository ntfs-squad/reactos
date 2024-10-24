/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file close APIs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdClose)
#endif

/* FUNCTIONS ****************************************************************/
extern PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;

_Function_class_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdClose(_In_ PDEVICE_OBJECT VolumeDeviceObject,
             _Inout_ PIRP Irp)
{
    /* Overview:
     * All instances of a file object have been closed.
     * Do any processing required and complete the IRP.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-close
     */

    // TODO: make this actually work
    PIO_STACK_LOCATION IrpSp;
    PFileContextBlock FileCB;

    if (VolumeDeviceObject == NtfsDiskFileSystemDeviceObject)
    {
        /* DeviceObject represents FileSystem */
        DPRINT1("Closing file system!\n");
        Irp->IoStatus.Information = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileCB = (PFileContextBlock)IrpSp->FileObject->FsContext;

    if (FileCB)
    {
        // Free BTree
        if (FileCB->FileDir)
            delete FileCB->FileDir;

        // Free file record
        if (FileCB->FileRec)
        {
            delete FileCB->FileRec->Data;
            delete FileCB->FileRec;
        }

        // Free the stream context block
        if (FileCB->StreamCB)
            delete FileCB->StreamCB;

        // Free the file context block
        delete FileCB;
    }

    Irp->IoStatus.Information = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DPRINT1("File closed!\n");
    return STATUS_SUCCESS;
}