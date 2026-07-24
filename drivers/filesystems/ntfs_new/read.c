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
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;
    PVolumeContextBlock VolCB;
    PNtfsVolume DiskVolume;
    NTSTATUS Status;
    NTSTATUS TimestampStatus;
    PUCHAR Buffer;
    LARGE_INTEGER ReadOffset;
    ULONG OriginalLength;
    ULONG RequestedLength;
    ULONG BytesRead;
    PFileContextBlock FileCB;
    PStandardInformationEx StdInfo;
    BOOLEAN ResourceAcquired = FALSE;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileObject = IrpSp->FileObject;
    VolCB = VolumeDeviceObject &&
            VolumeDeviceObject->DeviceExtension
        ? (PVolumeContextBlock)
            VolumeDeviceObject->DeviceExtension
        : NULL;
    DiskVolume = VolCB ? VolCB->DiskVolume : NULL;
    Buffer = (PUCHAR)(GetBuffer(Irp));
    ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    RequestedLength = IrpSp->Parameters.Read.Length;
    OriginalLength = RequestedLength;
    FileCB = FileObject
        ? (PFileContextBlock)FileObject->FsContext
        : NULL;

    if (!FileCB || !FileCB->FileRec || !DiskVolume)
    {
        DPRINT1("NtfsFsdRead(): invalid file or volume context!\n");
        Status = STATUS_INVALID_PARAMETER;
        goto Complete;
    }
    if (RequestedLength != 0 && !Buffer)
    {
        Status = STATUS_INVALID_USER_BUFFER;
        goto Complete;
    }
    if (ReadOffset.QuadPart < 0)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Complete;
    }

    ExAcquireResourceSharedLite(
        &FileCB->MainResource,
        TRUE);
    ResourceAcquired = TRUE;

    // Ensure the file has a valid resident StandardInformation attribute
    Status = NtfsFileRecordGetAttributeData(FileCB->FileRec,
                                            TypeStandardInformation,
                                            NULL,
                                            (PUCHAR*)&StdInfo);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("NtfsFsdRead(): Missing or corrupt $STANDARD_INFORMATION attribute!\n");
        goto Complete;
    }

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

    ExReleaseResourceLite(&FileCB->MainResource);
    ResourceAcquired = FALSE;

    if (NT_SUCCESS(Status))
    {
        BytesRead = OriginalLength - RequestedLength;
        if (BytesRead != 0 &&
            FileCB->RequestedType == TypeData &&
            !NtfsVolumeIsReadOnly(DiskVolume))
        {
            /*
             * Last-access is metadata, so serialize its MFT update after the
             * shared data read. A timestamp failure must not discard bytes
             * already delivered successfully to the caller.
             */
            ExAcquireResourceExclusiveLite(
                &FileCB->MainResource,
                TRUE);
            TimestampStatus =
                NtfsFileRecordUpdateAutomaticTimestamps(
                    FileCB->FileRec,
                    NTFS_BASIC_INFO_LAST_ACCESS_TIME);
            ExReleaseResourceLite(
                &FileCB->MainResource);
            if (!NT_SUCCESS(TimestampStatus))
            {
                DPRINT1(
                    "NtfsFsdRead(): failed to update last-access time: 0x%08lx\n",
                    TimestampStatus);
            }
        }

        if (FileObject->Flags & FO_SYNCHRONOUS_IO)
        {
            FileObject->CurrentByteOffset.QuadPart =
                ReadOffset.QuadPart + BytesRead;
        }

        Irp->IoStatus.Information = BytesRead;
    }

    else
    {
        Irp->IoStatus.Information = 0;
    }

Complete:
    if (ResourceAcquired)
        ExReleaseResourceLite(
            &FileCB->MainResource);
    if (!NT_SUCCESS(Status))
        Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
