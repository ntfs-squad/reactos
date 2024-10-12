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

    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;
    PUCHAR Buffer;
    LARGE_INTEGER ReadOffset;
    ULONG RequestedLength;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Buffer = (PUCHAR)(GetUserBuffer(Irp));
    ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    RequestedLength = IrpSp->Parameters.Read.Length;

    Status = ReadFile((PFileContextBlock)IrpSp->FileObject->FsContext,
                      ReadOffset.QuadPart,
                      RequestedLength,
                      Buffer);

    if (NT_SUCCESS(Status))
    {
        if (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            IrpSp->FileObject->CurrentByteOffset.QuadPart =
                ReadOffset.QuadPart + (IrpSp->Parameters.Read.Length - RequestedLength);
        }

        Irp->IoStatus.Information = IrpSp->Parameters.Read.Length - RequestedLength;
    }

    else
    {
        Irp->IoStatus.Information = NULL;
    }

    return Status;
}