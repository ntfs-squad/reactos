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
NTSTATUS
NTAPI
NtfsFsdRead(_In_ PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp)
{
    /* Overview:
     * Handles read requests.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-read
     */

    NTSTATUS Status;
    ULONG Information = 0;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PUCHAR Buffer;
    LARGE_INTEGER ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    ULONG RequestedLength = IrpSp->Parameters.Read.Length;
    PFileContextBlock FileCB = (PFileContextBlock)IrpSp->FileObject->FsContext;
    PVolumeContextBlock VolCB = VolumeDeviceObject->DeviceExtension;
    PStandardInformationEx StdInfo;

    if (!FileCB)
    {
        DPRINT1("INVESTIGATE ME: NtfsFsdRead() called with NULL FileCB!\n");
        Status = STATUS_INVALID_PARAMETER;
        goto Done;
    }

    if (FileCB->IsVolume)
    {
        ULONG BytesPerSector = NtfsVolumeGetBytesPerSector(VolCB->DiskVolume);

        if (ReadOffset.QuadPart < 0 || !BytesPerSector ||
            ReadOffset.QuadPart % BytesPerSector || RequestedLength % BytesPerSector)
        {
            Status = STATUS_INVALID_PARAMETER;
            goto Done;
        }

        if (!RequestedLength)
        {
            Status = STATUS_SUCCESS;
            goto Done;
        }

        ULONGLONG VolumeSize = NtfsVolumeGetSize(VolCB->DiskVolume);

        if (ReadOffset.QuadPart >= VolumeSize)
        {
            Status = STATUS_END_OF_FILE;
            goto Done;
        }

        /* Subtract before comparing to avoid overflowing ReadOffset + RequestedLength. */
        if ((ULONGLONG)RequestedLength > (ULONGLONG)(VolumeSize - ReadOffset.QuadPart))
            RequestedLength = (ULONG)(VolumeSize - ReadOffset.QuadPart);

        /* The volume device uses neither I/O. Lock the caller's pages before
         * handing a system mapping to the storage stack's synchronous IRP. */
        if (!Irp->MdlAddress && !Irp->AssociatedIrp.SystemBuffer)
        {
            if (!IoAllocateMdl(Irp->UserBuffer, RequestedLength, FALSE, FALSE, Irp))
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Done;
            }

            Status = STATUS_SUCCESS;
            _SEH2_TRY
            {
                MmProbeAndLockPages(Irp->MdlAddress, Irp->RequestorMode, IoWriteAccess);
            }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
            {
                Status = _SEH2_GetExceptionCode();
            }
            _SEH2_END;

            if (!NT_SUCCESS(Status))
            {
                IoFreeMdl(Irp->MdlAddress);
                Irp->MdlAddress = NULL;
                goto Done;
            }
            /* The I/O manager unlocks and frees this MDL when Irp completes. */
        }
    }

    Buffer = (PUCHAR)(GetBuffer(Irp));
    if (FileCB->IsVolume && !Buffer)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    if (FileCB->IsVolume)
    {
        IO_STATUS_BLOCK IoStatus;
        KEVENT Event;
        PIRP ReadIrp;

        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        ReadIrp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                                              VolCB->StorageDevice,
                                              Buffer,
                                              RequestedLength,
                                              &ReadOffset,
                                              &Event,
                                              &IoStatus);
        if (!ReadIrp)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Done;
        }

        Status = IoCallDriver(VolCB->StorageDevice, ReadIrp);
        if (Status == STATUS_PENDING)
        {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatus.Status;
        }

        if (NT_SUCCESS(Status))
            Information = IoStatus.Information;
        goto CompleteRead;
    }

    ASSERT(FileCB->FileRec);

    // Ensure the file has a valid resident StandardInformation attribute
    Status = NtfsFileRecordGetAttributeData(FileCB->FileRec,
                                            TypeStandardInformation,
                                            NULL,
                                            (PUCHAR*)&StdInfo);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("NtfsFsdRead(): Missing or corrupt $STANDARD_INFORMATION attribute!\n");
        goto Done;
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
        Information = IrpSp->Parameters.Read.Length - RequestedLength;

CompleteRead:
    if (NT_SUCCESS(Status) && (IrpSp->FileObject->Flags & FO_SYNCHRONOUS_IO))
        IrpSp->FileObject->CurrentByteOffset.QuadPart = ReadOffset.QuadPart + Information;

Done:
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
