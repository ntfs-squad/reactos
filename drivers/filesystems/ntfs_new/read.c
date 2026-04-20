/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

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

    if (!FileCB)
    {
        DPRINT1("INVESTIGATE ME: NtfsFsdRead() called with NULL FileCB!\n");
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    ASSERT(FileCB->FileRec);

    // Ensure the file has a valid resident StandardInformation attribute
    {
        
        PAttribute StdAttr = NtfsFileRecordGetAttribute(FileCB->FileRec,
                                                        TypeStandardInformation,
                                                        NULL);
        if (!StdAttr || StdAttr->IsNonResident)
        {
            DPRINT1("NtfsFsdRead(): Missing or invalid $STANDARD_INFORMATION attribute!\n");
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
            return STATUS_FILE_CORRUPT_ERROR;
        }
        // Validate resident data window
        if (StdAttr->Resident.DataOffset < 0x18 ||
            (StdAttr->Resident.DataOffset + StdAttr->Resident.DataLength) > StdAttr->Length)
        {
            DPRINT1("NtfsFsdRead(): Corrupt $STANDARD_INFORMATION resident layout!\n");
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
            return STATUS_FILE_CORRUPT_ERROR;
        }
        StdInfo = (PStandardInformationEx) GetResidentDataPointer(StdAttr);
    }

    ASSERT(!(StdInfo->FilePermissions & FILE_PERM_COMPRESSED));
    ASSERT(!(StdInfo->FilePermissions & FILE_PERM_ENCRYPTED));

    // TODO: Investigate minor function before reading
    if (IrpSp->MinorFunction == IRP_MN_COMPLETE)
        __debugbreak();

    if (RequestedLength)
    {
        // Copy data from $DATA into file buffer.
        Status = NtfsFileRecordCopyData(FileCB->FileRec,
                                        FileCB->RequestedType,
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
        if (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            IrpSp->FileObject->CurrentByteOffset.QuadPart =
                ReadOffset.QuadPart + IrpSp->Parameters.Read.Length - RequestedLength;
        }

        Irp->IoStatus.Information = IrpSp->Parameters.Read.Length - RequestedLength;
    }

    else
    {
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
