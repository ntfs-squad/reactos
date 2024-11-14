/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new write APIs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

/* INCLUDES *****************************************************************/

#include "ntfspch.h"

/* GLOBALS *****************************************************************/

/* FUNCTIONS ****************************************************************/

_Function_class_(IRP_MJ_WRITE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdWrite(_In_ PDEVICE_OBJECT VolumeDeviceObject,
             _Inout_ PIRP Irp)
{
    /* Overview:
     * Handles write requests.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-write
     */
    UNREFERENCED_PARAMETER(VolumeDeviceObject);

    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PUCHAR Buffer;
    ULONGLONG ByteOffset;
    ULONG Length;
    PFileContextBlock FileCB;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Buffer = (PUCHAR)(GetBuffer(Irp));
    ByteOffset = IrpSp->Parameters.Write.ByteOffset.QuadPart;
    Length = IrpSp->Parameters.Write.Length;
    FileCB = (PFileContextBlock)IrpSp->FileObject->FsContext;

    Status = FileCB->FileRec->WriteFileData(NULL, Buffer, &Length, ByteOffset);

    if (NT_SUCCESS(Status))
    {
        // I don't know yet what to do here...
        __debugbreak();
    }

    else
    {
        Irp->IoStatus.Information = NULL;
    }

    return Status;
}