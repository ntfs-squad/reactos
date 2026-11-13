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
    ULONG Information = 0;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PUCHAR Buffer = (PUCHAR)(GetBuffer(Irp));
    LARGE_INTEGER ByteOffset = IrpSp->Parameters.Write.ByteOffset;
    ULONG Length = IrpSp->Parameters.Write.Length;
    PFILE_OBJECT FileObj = IrpSp->FileObject;
    PFileContextBlock FileCB = (PFileContextBlock)FileObj->FsContext;
    PNtfsVolume DiskVolume;
    PNtfsFileRecord FileRec;
    PNtfsMasterFileTable Mft;
    AttributeType RequestedType;
    PWSTR RequestedStream;

    if (FileCB && FileCB->IsVolume)
    {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto Done;
    }

    DiskVolume = ((PVolumeContextBlock)VolumeDeviceObject->DeviceExtension)->DiskVolume;
    Mft = NtfsVolumeGetMft(DiskVolume);

    DPRINT1("NtfsFsdWrite() called!\n");

    if (NtfsVolumeIsReadOnly(DiskVolume))
    {
        // Disk is read-only. Don't try to write anything.
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto Done;
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
        Status = NtfsMasterFileTableGetFileRecordFromQuery(Mft,
                                                           FileObj->FileName.Buffer,
                                                           &FileRec);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to find file record!\n");
            Status = STATUS_INVALID_PARAMETER;
            goto Done;
        }

        Status = NtfsVolumeGetADSPreference(DiskVolume,
                                            FileObj,
                                            &RequestedType,
                                            &RequestedStream);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to find ADS preferences!\n");
            ExFreePool(FileRec);
            Status = STATUS_INVALID_PARAMETER;
            goto Done;
        }
    }

    Status = NtfsFileRecordWriteFileData(FileRec,
                                         RequestedType,
                                         RequestedStream,
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

        Information = Length;
    }

Done:
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
