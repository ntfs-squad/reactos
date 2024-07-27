/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

/* FUNCTIONS ****************************************************************/

// TODO: Move me somewhere else!
PVOID
GetUserBuffer(PIRP Irp,
                  BOOLEAN Paging)
{
    if (Irp->MdlAddress != NULL)
    {
        return MmGetSystemAddressForMdlSafe(Irp->MdlAddress, (Paging ? HighPagePriority : NormalPagePriority));
    }
    else
    {
        return Irp->UserBuffer;
    }
}

NTSTATUS
ReadFile(_In_ PIO_STACK_LOCATION IoStack,
         _Out_ PVOID Buffer,
         _In_ ULONG RequestedLength,
         _Out_ PULONG ReadLength)
{
    NTSTATUS Status;
    PFileContextBlock FileCB;
    ResidentAttribute* StdInfoAttr;
    StandardInformationEx* StdInfoAttrEx;

    // If we aren't reading anything, don't read anything.
    if (!RequestedLength)
        return STATUS_SUCCESS;

    FileCB = (PFileContextBlock)IoStack->FileObject->FsContext;

    DPRINT1("File Context Block found!\n");

    if (!FileCB)
        DPRINT1("File context block is invalid!\n");

    // If there is no file record, we can't find the file.
    if (!FileCB->FileRec)
        return STATUS_FILE_NOT_AVAILABLE;

    DPRINT1("File record found!\n");

    // Initialize variables.
    StdInfoAttr = new(NonPagedPool) ResidentAttribute();
    StdInfoAttrEx = new(NonPagedPool) StandardInformationEx();

    // Get standard information for file.
    FileCB->FileRec->FindAttribute(StandardInformation, NULL, StdInfoAttr, (PUCHAR)StdInfoAttrEx);

    // Check if file is compressed.
    if (StdInfoAttrEx->FilePermissions & FILE_PERM_COMPRESSED)
    {
        UNIMPLEMENTED;
        Status = STATUS_NOT_IMPLEMENTED;
        goto cleanup;
    }

    // Check if file is encrypted.
    if(StdInfoAttrEx->FilePermissions & FILE_PERM_ENCRYPTED)
    {
        UNIMPLEMENTED;
        Status = STATUS_NOT_IMPLEMENTED;
        goto cleanup;
    }

    // TODO: COMPLETE!!!
    Status = FileCB->FileRec->FindAttribute(Data, NULL, NULL, (PUCHAR)Buffer);

cleanup:
    if (StdInfoAttr)
        delete StdInfoAttr;
    if (StdInfoAttrEx)
        delete StdInfoAttrEx;
    return Status;
}

_Function_class_(IRP_MJ_READ)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdRead(_In_ PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp)
{
    /* Overview:
     * Handles read requests.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-read
     */
    UNREFERENCED_PARAMETER(VolumeDeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    PIO_STACK_LOCATION IoStack;
    NTSTATUS Status;
    PVOID Buffer;
    ULONG ReadLength;
    LARGE_INTEGER ReadOffset;
    ULONG RequestedLength;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    Buffer = GetUserBuffer(Irp, BooleanFlagOn(Irp->Flags, IRP_PAGING_IO));
    ReadOffset = IoStack->Parameters.Read.ByteOffset;
    RequestedLength = IoStack->Parameters.Read.Length;

    Status = ReadFile(IoStack,
                      Buffer,
                      RequestedLength,
                      &ReadLength);

    if (NT_SUCCESS(Status))
    {
        if (IoStack->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            IoStack->FileObject->CurrentByteOffset.QuadPart =
                ReadOffset.QuadPart + ReadLength;
        }

        Irp->IoStatus.Information = ReadLength;
    }
    else
    {
        Irp->IoStatus.Information = NULL;
    }

    return Status;
}