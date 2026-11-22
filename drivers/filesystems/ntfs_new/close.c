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
#pragma alloc_text(PAGE, NtfsDestroyFileContextBlock)
#pragma alloc_text(PAGE, NtfsFsdClose)
#endif

/* FUNCTIONS ****************************************************************/
NTSTATUS
NtfsDestroyFileContextBlock(_In_ PIRP Irp)
{
    PFILE_OBJECT FileObject;
    PFileContextBlock FileCB;

    PAGED_CODE();

    FileObject = IoGetCurrentIrpStackLocation(Irp)->FileObject;
    if (!FileObject)
        return STATUS_INVALID_PARAMETER;

    FileCB = FileObject->FsContext;
    if (!FileCB)
        return STATUS_INVALID_PARAMETER;

    if (FileObject->SectionObjectPointer == &FileCB->SectionObjectPointers)
    {
        if (FileObject->PrivateCacheMap)
            CcUninitializeCacheMap(FileObject, NULL, NULL);
        FileObject->SectionObjectPointer = NULL;
    }
    FileObject->FsContext = NULL;

    NtfsDirectoryDestroy(FileCB->FileDir);
    NtfsFileRecordDestroy(FileCB->FileRec);
    if (FileCB->StreamCB)
        ExFreePool(FileCB->StreamCB);
    if (FileCB->FileName.Buffer)
        ExFreePool(FileCB->FileName.Buffer);
    if (FileCB->RequestedStream)
        ExFreePool(FileCB->RequestedStream);
    FsRtlUninitializeFileLock(&FileCB->FileLock);
    if (FileCB->CommonFCBHeader.PagingIoResource)
        ExDeleteResourceLite(FileCB->CommonFCBHeader.PagingIoResource);
    if (FileCB->CommonFCBHeader.Resource)
        ExDeleteResourceLite(FileCB->CommonFCBHeader.Resource);
    ExFreePool(FileCB);

    return STATUS_SUCCESS;
}

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

    NTSTATUS Status = NtfsDestroyFileContextBlock(Irp);

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
