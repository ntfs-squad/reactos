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