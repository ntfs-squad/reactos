/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

/* INCLUDES *****************************************************************/
#include "ntfspch.h"

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
    PFileContextBlock FileCB;
    PStandardInformationEx StdInfo;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Buffer = (PUCHAR)(GetBuffer(Irp));
    ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    RequestedLength = IrpSp->Parameters.Read.Length;
    FileCB = (PFileContextBlock)IrpSp->FileObject->FsContext;

    ASSERT(FileCB);
    ASSERT(FileCB->FileRec);

    // For now until I add support for these
    StdInfo = (PStandardInformationEx)
              GetResidentDataPointer(FileCB->FileRec->GetAttribute(TypeStandardInformation,
                                     NULL));

    ASSERT(!(StdInfo->FilePermissions & FILE_PERM_COMPRESSED));
    ASSERT(!(StdInfo->FilePermissions & FILE_PERM_ENCRYPTED));

    // TODO: Investigate minor function before reading
    if (IrpSp->MinorFunction == IRP_MN_COMPLETE)
        __debugbreak();

    if (RequestedLength)
    {
        // Copy data from $DATA into file buffer.
        Status = FileCB->FileRec->CopyData(FileCB->RequestedType,
                                           FileCB->RequestedStream,
                                           Buffer,
                                           &RequestedLength,
                                           ReadOffset.QuadPart);
    }

    else
    {
        // If we aren't reading anything, don't read anything.
        Status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(Status))
    {
        DPRINT1("RequestedLength: %ld\n", RequestedLength);
        DPRINT1("IrpSp->Parameters.Read.Length: %ld\n", IrpSp->Parameters.Read.Length);

        if (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            IrpSp->FileObject->CurrentByteOffset.QuadPart =
                ReadOffset.QuadPart + IrpSp->Parameters.Read.Length - RequestedLength;
        }

        Irp->IoStatus.Information = IrpSp->Parameters.Read.Length - RequestedLength;
        DPRINT1("Irp->IoStatus.Information: %ld\n", Irp->IoStatus.Information);
    }

    else
    {
        Irp->IoStatus.Information = NULL;
    }

    return Status;
}