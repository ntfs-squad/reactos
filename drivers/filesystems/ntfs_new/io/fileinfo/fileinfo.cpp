/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file information
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "fileinfo.h"

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

    DPRINT1("NtfsFsdQueryInformation Called!\n");

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    FileInfoRequest = IoStack->Parameters.QueryFile.FileInformationClass;
    FileObject = IoStack->FileObject;
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    FileCB = (PFileContextBlock)FileObject->FsContext;
    SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IoStack->Parameters.QueryFile.Length;

    switch (FileInfoRequest)
    {
        case FileBasicInformation:
            Status = GetFileBasicInformation(FileCB,
                                             (PFILE_BASIC_INFORMATION)SystemBuffer,
                                             &BufferLength);
            break;
        case FileNameInformation:
            Status = GetFileNameInformation(FileCB,
                                            (PFILE_NAME_INFORMATION)SystemBuffer,
                                            &BufferLength);
            DPRINT1("Buffer Contents: \"%S\", Length: %ld\n", ((PFILE_NAME_INFORMATION)SystemBuffer)->FileName, ((PFILE_NAME_INFORMATION)SystemBuffer)->FileNameLength);
            break;
        default:
            DPRINT1("Unhandled File Information Request %d!\n", FileInfoRequest);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    if (NT_SUCCESS(Status))
        Irp->IoStatus.Information =
            IoStack->Parameters.QueryFile.Length - BufferLength;
    else
        Irp->IoStatus.Information = 0;

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

    // TODO: make this actually work
    UNREFERENCED_PARAMETER(VolumeDeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    DPRINT1("Called NtfsFsdDirectoryControl() which is a STUB!\n");
    return STATUS_NOT_IMPLEMENTED;
}
