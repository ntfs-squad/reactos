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
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PUCHAR Buffer;
    LARGE_INTEGER ByteOffset;
    ULONG Length;
    ULONG RequestedLength;
    PFileContextBlock FileCB;
    PFILE_OBJECT FileObj;
    PNTFSVolume Volume;
    PFileRecord FileRec;
    AttributeType RequestedType;
    PWSTR RequestedStream;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Buffer = (PUCHAR)(GetBuffer(Irp));
    ByteOffset = IrpSp->Parameters.Write.ByteOffset;
    Length = IrpSp->Parameters.Write.Length;
    RequestedLength = Length;
    FileObj = IrpSp->FileObject;
    FileCB = (PFileContextBlock)FileObj->FsContext;
    Volume = ((PVolumeContextBlock)VolumeDeviceObject->DeviceExtension)->Volume;

    DPRINT1("NtfsFsdWrite() called!\n");

    if (Volume->IsReadOnly)
    {
        // Disk is read-only. Don't try to write anything.
        Irp->IoStatus.Information = 0;
        Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
        return Status;
    }

    // Sometimes the file context block is still available, and sometimes it's not.
    if (FileCB)
    {
        FileRec = FileCB->FileRec;
        RequestedType = FileCB->RequestedType;
        RequestedStream = FileCB->RequestedStream;

        // Set the offset to end of file if FILE_APPEND_DATA is set
        if (FileCB->DesiredAccess == FILE_APPEND_DATA)
        {
            ByteOffset.HighPart = -1;
            ByteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;
        }
    }

    else
    {
        Status = Volume->MFT->GetFileRecordFromQuery(FileObj->FileName.Buffer,
                                                     &FileRec);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to find file record!\n");
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
            return STATUS_INVALID_PARAMETER;
        }

        Status = Volume->GetADSPreference(FileObj,
                                          &RequestedType,
                                          &RequestedStream);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to find ADS preferences!\n");
            delete FileRec;
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
            return STATUS_INVALID_PARAMETER;
        }
    }

    Status = FileRec->WriteFileData(RequestedType,
                                    RequestedStream,
                                    Buffer,
                                    &Length,
                                    &ByteOffset);

    if (NT_SUCCESS(Status))
    {
        if (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            // Advance file pointer
            ULONG bytesWritten = RequestedLength - Length;
            IrpSp->FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + bytesWritten;
        }

        Irp->IoStatus.Information = RequestedLength - Length;
    }
    else
    {
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
