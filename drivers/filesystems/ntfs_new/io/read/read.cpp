/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/
#include "read.h"

/* GLOBALS *****************************************************************/

/* FUNCTIONS ****************************************************************/
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

    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;
    PUCHAR Buffer;
    ULONG ReadLength;
    LARGE_INTEGER ReadOffset;
    ULONG RequestedLength;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Buffer = GetUserBuffer(Irp, BooleanFlagOn(Irp->Flags, IRP_PAGING_IO));
    ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    RequestedLength = IrpSp->Parameters.Read.Length;

    DPRINT1("Incoming read request!\n");
    DPRINT1("Requested Length: 0x%X\n", RequestedLength);
    DPRINT1("Read offset: 0x%X\n", ReadOffset);

    // TODO: Consider axing the read length parameter
    Status = ReadFile((PFileContextBlock)IrpSp->FileObject->FsContext,
                      ReadOffset.QuadPart,
                      RequestedLength,
                      Buffer,
                      &ReadLength);

    if (NT_SUCCESS(Status))
    {
        if (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            IrpSp->FileObject->CurrentByteOffset.QuadPart =
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