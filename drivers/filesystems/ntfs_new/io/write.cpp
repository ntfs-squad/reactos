/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new write APIs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

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
    LARGE_INTEGER ByteOffset;
    ULONG Length;
    PFileContextBlock FileCB;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Buffer = (PUCHAR)(GetBuffer(Irp));
    ByteOffset = IrpSp->Parameters.Write.ByteOffset;
    Length = IrpSp->Parameters.Write.Length;
    FileCB = (PFileContextBlock)IrpSp->FileObject->FsContext;

    // Set the offset to end of file if FILE_APPEND_DATA is set
    if (FileCB->DesiredAccess == FILE_APPEND_DATA)
    {
        ByteOffset.HighPart = -1;
        ByteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;
    }

    Status = FileCB->FileRec->WriteFileData(FileCB->RequestedType,
                                            FileCB->RequestedStream,
                                            Buffer,
                                            &Length,
                                            &ByteOffset);

    if (NT_SUCCESS(Status))
    {
        if (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            // Advance file pointer
            IrpSp->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + Length;
        }

        Irp->IoStatus.Information = Length;
    }

    else
    {
        Irp->IoStatus.Information = NULL;
    }

    return Status;
}
