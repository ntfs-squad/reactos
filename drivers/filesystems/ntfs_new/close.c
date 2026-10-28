/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file close APIs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */


#include "ntfspch.h"

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdClose)
#endif

/* FUNCTIONS ****************************************************************/

_Function_class_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
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

    if (VolumeDeviceObject == NtfsDiskFileSystemDeviceObject)
    {
        /* DeviceObject represents FileSystem */
        DPRINT1("Closing global NTFS!\n");
        Irp->IoStatus.Information = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    /* Perform final teardown of the file object's context. */
    {
        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
        PFileContextBlock FileCB = (PFileContextBlock)IrpSp->FileObject->FsContext;
        if (FileCB)
        {
            // Uninitialize cache map if present
            if (IrpSp->FileObject->PrivateCacheMap)
            {
                CcUninitializeCacheMap(IrpSp->FileObject, NULL, NULL);
            }

            ExDeleteResourceLite(&FileCB->MainResource);
            ExDeleteResourceLite(&FileCB->PagingIoResource);
            FsRtlUninitializeFileLock(&FileCB->FileLock);

            if (FileCB->FileDir)
                ExFreePool(FileCB->FileDir);

            if (FileCB->FileRec)
                ExFreePool(FileCB->FileRec);

            if (FileCB->StreamCB)
                ExFreePool(FileCB->StreamCB);

            if (FileCB->FileName.Buffer)
                ExFreePool(FileCB->FileName.Buffer);

            ExFreePool(FileCB);
            IrpSp->FileObject->FsContext = NULL;
        }
    }

    Irp->IoStatus.Information = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}
