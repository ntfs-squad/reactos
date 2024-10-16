/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file information
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "fileinfo.h"
#include "filebothdir.h"

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

    /* Overview:
     * Determine if file information request is appropriate.
     * If it is, fulfill it. If it isn't, return STATUS_INVALID_DEVICE_REQUEST.
     *
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mj-query-information
     */

    PIO_STACK_LOCATION IoStack;
    FILE_INFORMATION_CLASS FileInfoRequest;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    NTSTATUS Status;
    PVOID SystemBuffer;
    PFILE_OBJECT FileObject;
    ULONG BufferLength;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    FileInfoRequest = IoStack->Parameters.QueryFile.FileInformationClass;
    FileObject = IoStack->FileObject;
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    FileCB = (PFileContextBlock)FileObject->FsContext;
    if (!FileCB)
    {
        DPRINT1("File not found!\n");
        Status = STATUS_NOT_FOUND;
        goto done;
    }

    FileObject->SectionObjectPointer = &(FileCB->StreamCB->SectionObjectPointers);

    SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IoStack->Parameters.QueryFile.Length;

    switch (FileInfoRequest)
    {
        case FileBasicInformation:
            DPRINT1("File basic information requested!\n");
            Status = GetFileBasicInformation(FileCB,
                                             (PFILE_BASIC_INFORMATION)SystemBuffer,
                                             &BufferLength);
            break;
        case FileStandardInformation:
            DPRINT1("File standard information requested!\n");
            Status = GetFileStandardInformation(FileCB,
                                                (PFILE_STANDARD_INFORMATION)SystemBuffer,
                                                &BufferLength);
            break;
        case FileInternalInformation:
            DPRINT1("File internal information requested!\n");
            Status = GetFileInternalInformation(FileCB,
                                                (PFILE_INTERNAL_INFORMATION)SystemBuffer,
                                                &BufferLength);
            break;
        case FileNameInformation:
            DPRINT1("File name information requested!\n");
            Status = GetFileNameInformation(FileCB,
                                            (PFILE_NAME_INFORMATION)SystemBuffer,
                                            &BufferLength);
            break;
        case FileNetworkOpenInformation:
            DPRINT1("FileNetworkOpenInformation requested!\n");
            Status = GetFileNetworkOpenInformation(FileCB,
                                                   (PFILE_NETWORK_OPEN_INFORMATION)SystemBuffer,
                                                   &BufferLength);
            break;
        default:
            DPRINT1("Unhandled file information request %d!\n", FileInfoRequest);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
done:
    if (NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = IoStack->Parameters.QueryFile.Length - BufferLength;

        // HACK!!! Why is this still needed?
        Irp->UserIosb->Status = Irp->IoStatus.Status;
        Irp->UserIosb->Information = Irp->IoStatus.Information;
    }
    else
    {
        Irp->IoStatus.Information = 0;
    }

    return Status;
}

_Function_class_(IRP_MJ_SET_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp)
{
    /* Overview:
     * Check if a requested file is open.
     * If it is, set information in the file as requested.
     *
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-set-information
     */

#if 0
    PIO_STACK_LOCATION IoStack;
    FILE_INFORMATION_CLASS FileInformationRequest;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    NTSTATUS Status;
    PVOID SystemBuffer;
    ULONG BufferLength;
    PFILE_OBJECT FileObject;
    ULONG BufferLength;

    DPRINT("NtfsSetInformation(%p)\n", IrpContext);

    Irp = IrpContext->Irp;
    IoStack = IrpContext->Stack;
    DeviceObject = IrpContext->DeviceObject;
    DeviceExt = DeviceObject->DeviceExtension;
    FileInformationRequest = Stack->Parameters.QueryFile.FileInformationClass;
    FileObject = IrpContext->FileObject;
    Fcb = FileObject->FsContext;

    SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    BufferLength = Stack->Parameters.QueryFile.Length;

    if (!ExAcquireResourceSharedLite(&Fcb->MainResource,
                                     BooleanFlagOn(IrpContext->Flags, IRPCONTEXT_CANWAIT)))
    {
        return NtfsMarkIrpContextForQueue(IrpContext);
    }

    switch (FileInformationRequest)
    {
        PFILE_END_OF_FILE_INFORMATION EndOfFileInfo;

        /* TODO: Allocation size is not actually the same as file end for NTFS,
           however, few applications are likely to make the distinction. */
        case FileAllocationInformation:
            DPRINT1("FIXME: Using hacky method of setting FileAllocationInformation.\n");
        case FileEndOfFileInformation:
            EndOfFileInfo = (PFILE_END_OF_FILE_INFORMATION)SystemBuffer;
            Status = NtfsSetEndOfFile(Fcb,
                                      FileObject,
                                      DeviceExt,
                                      Irp->Flags,
                                      BooleanFlagOn(Stack->Flags, SL_CASE_SENSITIVE),
                                      &EndOfFileInfo->EndOfFile);
            break;

        // TODO: all other information classes

        default:
            DPRINT1("FIXME: Unimplemented information class: %s\n", GetInfoClassName(FileInformationClass));
            Status = STATUS_NOT_IMPLEMENTED;
    }
#endif
    DPRINT1("NtfsFsdSetInformation Called!\n");
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
    /* Overview:
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-directory-control
     */

    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    FILE_INFORMATION_CLASS FileInformationRequest;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    PVOID SystemBuffer;
    ULONG BufferLength;
    BOOLEAN ReturnSingleEntry;

    DPRINT1("NtfsDirectoryControl() called\n");

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileCB = (PFileContextBlock)(IrpSp->FileObject->FsContext);
    VolCB = (PVolumeContextBlock)(VolumeDeviceObject->DeviceExtension);
    SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IrpSp->Parameters.QueryDirectory.Length;

    if (!SystemBuffer)
        __debugbreak();

    if (IrpSp->MinorFunction == IRP_MN_QUERY_DIRECTORY)
    {
        FileInformationRequest = IrpSp->Parameters.QueryDirectory.FileInformationClass;
        ReturnSingleEntry = !!(IrpSp->Flags & SL_RETURN_SINGLE_ENTRY);

        switch(FileInformationRequest)
        {
            case FileBothDirectoryInformation:
                DPRINT1("FileBothDirectoryInformation request!\n");
                Status = GetFileBothDirectoryInformation(FileCB,
                                                         VolCB,
                                                         ReturnSingleEntry,
                                                         (PFILE_BOTH_DIR_INFORMATION)SystemBuffer,
                                                         &BufferLength);
                break;
            case FileDirectoryInformation:
                DPRINT1("FileDirectoryInformation request!\n");
                break;
            case FileFullDirectoryInformation:
                DPRINT1("FileFullDirectoryInformation request!\n");
                break;
            case FileIdBothDirectoryInformation:
                DPRINT1("FileIdBothDirectoryInformation request!\n");
                break;
            case FileIdFullDirectoryInformation:
                DPRINT1("FileIdFullDirectoryInformation request!\n");
                break;
            case FileNamesInformation:
                DPRINT1("FileNamesInformation request!\n");
                break;
            case FileObjectIdInformation:
                DPRINT1("FileObjectIdInformation request!\n");
                break;
            case FileReparsePointInformation:
                DPRINT1("FileReparsePointInformation request!\n");
                break;
            default:
                DPRINT1("Unknown directory query request! %lu\n", FileInformationRequest);
                break;
        }
    }

    else
    {
        if (IrpSp->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY)
        {
            DPRINT1("IRP_MN_NOTIFY_CHANGE_DIRECTORY\n");
            Status = STATUS_SUCCESS;
        }

        else
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    // Set to number of bytes written
    if (NT_SUCCESS(Status))
    {
        Irp->IoStatus.Information = IrpSp->Parameters.QueryDirectory.Length - BufferLength;
        DPRINT1("Buffer Length: %lu\nRemaining Buffer Length: %lu\nI/O Status Info: %lu\n",
                IrpSp->Parameters.QueryDirectory.Length,
                BufferLength, Irp->IoStatus.Information);
    }
    else
    {
        Irp->IoStatus.Information = 0;
    }

    return Status;
}
