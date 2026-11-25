/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Kernelmode glue
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include <ntifs.h>
#include <ntfs_km.h>
#include "../../include/internal/ntfslib_new_internal_alloc.h"

#ifndef TAG_NTFS
#define TAG_NTFS 'NTFS'
#endif

/* We can only transfer what can be represented in one MDL at a time. The
 * maximum size that can be represented in one MDL varies by NT version and
 * architecture.
 *
 * The calculation for Windows XP/Windows Server 2003/NT5.2 and older is:
 *     PAGE_SIZE * (MaxMDLPayloadSize / sizeof(PFN_NUMBER)), where:
 *         - MaxMDLPayloadSize = MAXUSHORT - sizeof(MDL)
 *     Note: MAXUSHORT is 65535, and PFN_NUMBER is a typedef of ULONG_PTR.
 *
 * The calculation for Windows Vista/Server 2008/NT6.0 is:
 *     2 GiB - PAGE_SIZE
 *
 * The calculation for Windows 7/Server 2008 R2/NT 6.1 and newer is:
 *     4 GiB - PAGE_SIZE
 *
 * See also: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioallocatemdl#remarks
 */
#if NTDDI_VERSION <= NTDDI_WS03SP4 || defined(__REACTOS__) // TODO: Get rid of this pin when ReactOS can support larger MDLs.
#define MDL_MAX_PAYLOAD_SIZE (MAXUSHORT - sizeof(MDL))
#define MAX_BYTES_PER_MDL ((ULONG)(PAGE_SIZE * (MDL_MAX_PAYLOAD_SIZE / sizeof(PFN_NUMBER))))
#elif NTDDI_VERSION <= NTDDI_VISTASP4
#define MAX_BYTES_PER_MDL ((ULONG)((1ULL << 31) - PAGE_SIZE))
#else
#define MAX_BYTES_PER_MDL ((ULONG)((1ULL << 32) - PAGE_SIZE))
#endif

PDEVICE_OBJECT PartDeviceObj = NULL;
ULONG BytesPerSector = 0;

#ifdef __cplusplus
extern "C" {
#endif

void*
NtfsAllocatePoolWithTag(NTFS_POOL PoolType, size_t Size, ULONG Tag)
{
    return ExAllocatePoolWithTag((POOL_TYPE)PoolType, Size, Tag);
}

void NtfsFreePool(void* pObject)
{
    ExFreePool(pObject);
}

void
NtfsFillMemory(_In_ PVOID Buffer,
               _In_ size_t Size,
               _In_ UCHAR Value)
{
    RtlFillMemory(Buffer, Size, Value);
}

#ifdef __cplusplus
}
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG SectorBytes)
{
    if (!DeviceObject || SectorBytes == 0)
        return STATUS_INVALID_PARAMETER;

    PartDeviceObj = DeviceObject;
    BytesPerSector = SectorBytes;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
BypassCompletion(_In_ PDEVICE_OBJECT DeviceObject,
                 _In_ PIRP Irp,
                 _In_ PVOID Context)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    /* Paging reads can wait at APC_LEVEL, where completion APCs cannot run.
     * Bypass the completion APC to avoid deadlock.
     */
    KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
AccessVolumeAligned(_In_ ULONG MajorFunction,
                    _In_ PDEVICE_OBJECT DeviceObject,
                    _In_ ULONGLONG Offset,
                    _In_ ULONG Length,
                    _Inout_ PUCHAR Buffer)
{
    PAGED_CODE();
    ASSERT(Offset % BytesPerSector == 0); /* Offset is sector aligned */
    ASSERT(Length % BytesPerSector == 0); /* Length is sector aligned */
    ASSERT(BYTE_OFFSET(Buffer) == 0);     /* Buffer is page aligned */
    ASSERT(MajorFunction == IRP_MJ_READ || MajorFunction == IRP_MJ_WRITE);

    while (Length)
    {
        KEVENT Event;
        IO_STATUS_BLOCK Iosb;
        LARGE_INTEGER ByteOffset;
        ULONG RequestedLength = min(Length, MAX_BYTES_PER_MDL);
        NTSTATUS Status;
        ULONG Information;
        PIRP Irp;

        ByteOffset.QuadPart = Offset;
        KeInitializeEvent(&Event, NotificationEvent, FALSE);

        Irp = IoBuildAsynchronousFsdRequest(MajorFunction,
                                            DeviceObject,
                                            Buffer,
                                            RequestedLength,
                                            &ByteOffset,
                                            &Iosb);
        if (!Irp)
            return STATUS_INSUFFICIENT_RESOURCES;

        SetFlag(IoGetNextIrpStackLocation(Irp)->Flags, SL_OVERRIDE_VERIFY_VOLUME);
        IoSetCompletionRoutine(Irp, BypassCompletion, &Event, TRUE, TRUE, TRUE);

        Status = IoCallDriver(DeviceObject, Irp);
        if (Status == STATUS_PENDING)
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);

        Iosb = Irp->IoStatus;
        Status = Iosb.Status;
        Information = Iosb.Information;

        if (!NT_SUCCESS(Status))
            goto Cleanup;

        if (Information != RequestedLength)
        {
            Status = STATUS_DEVICE_DATA_ERROR;
            goto Cleanup;
        }

        /* Buffered reads return data to the IRP system buffer instead of our own.
         * Copy the IRP system buffer into our buffer before freeing the IRP.
         */
        if ((Irp->Flags & IRP_BUFFERED_IO) && (Irp->Flags & IRP_INPUT_OPERATION))
            RtlCopyMemory(Buffer, Irp->AssociatedIrp.SystemBuffer, Information);

Cleanup:
        // Free IRP buffer, MDLs, and the IRP
        if (Irp->Flags & IRP_DEALLOCATE_BUFFER)
            ExFreePool(Irp->AssociatedIrp.SystemBuffer);
        while (Irp->MdlAddress)
        {
            PMDL Mdl = Irp->MdlAddress;
            Irp->MdlAddress = Mdl->Next;
            MmUnlockPages(Mdl);
            IoFreeMdl(Mdl);
        }
        IoFreeIrp(Irp);

        if (!NT_SUCCESS(Status))
            return Status;

        Offset += RequestedLength;
        Buffer += RequestedLength;
        Length -= RequestedLength;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
AccessVolume(_In_ ULONG MajorFunction,
             _In_ ULONGLONG Offset,
             _In_ ULONG Length,
             _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status;
    PUCHAR TemporaryBuffer;
    ULONGLONG AlignedOffset, AlignedLength;

    ASSERT(MajorFunction == IRP_MJ_READ || MajorFunction == IRP_MJ_WRITE);

    if (!Length)
        return STATUS_SUCCESS;

    AlignedOffset = Offset - (Offset % BytesPerSector);
    AlignedLength = ALIGN_UP_BY((ULONGLONG)(Offset % BytesPerSector) + Length,
                                BytesPerSector);

    // Optimization: if the read/write is already page and sector aligned, do it directly.
    if (AlignedOffset == Offset
        && AlignedLength == Length
        && BYTE_OFFSET(Buffer) == 0)
    {
        return AccessVolumeAligned(MajorFunction,
                                   PartDeviceObj,
                                   AlignedOffset,
                                   AlignedLength,
                                   Buffer);
    }

    /* Allocations of at least PAGE_SIZE are page-aligned.
     * Ensure the temporary buffer is page aligned by allocating at least a page.
     */
    TemporaryBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool,
                                                    max((SIZE_T)AlignedLength,
                                                        (SIZE_T)PAGE_SIZE),
                                                    TAG_NTFS);
    if (!TemporaryBuffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    // Optimization: If we are writing to an already sector aligned region, we can skip the disk read.
    if (MajorFunction == IRP_MJ_WRITE
        && AlignedOffset == Offset
        && AlignedLength == Length)
    {
        goto Complete;
    }

    // Fill the temporary buffer with an aligned read.
    Status = AccessVolumeAligned(IRP_MJ_READ,
                                 PartDeviceObj,
                                 AlignedOffset,
                                 AlignedLength,
                                 TemporaryBuffer);

    if (!NT_SUCCESS(Status))
        goto Done;

Complete:
    if (MajorFunction == IRP_MJ_READ)
    {
        RtlCopyMemory(Buffer,
                      TemporaryBuffer + (Offset % BytesPerSector),
                      Length);
    }

    else /* MajorFunction == IRP_MJ_WRITE */
    {
        RtlCopyMemory(TemporaryBuffer + (Offset % BytesPerSector),
                      Buffer,
                      Length);

        Status = AccessVolumeAligned(MajorFunction,
                                     PartDeviceObj,
                                     AlignedOffset,
                                     AlignedLength,
                                     TemporaryBuffer);
    }

Done:
    ExFreePool(TemporaryBuffer);
    return Status;
}

#ifdef __cplusplus
extern "C" {
#endif

VOID
NtfsAssertPagedCode(VOID)
{
    PAGED_CODE();
}

ULONG
NtfsGetBytesPerSector(VOID)
{
    return BytesPerSector;
}

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    return AccessVolume(IRP_MJ_READ, Offset, Length, Buffer);
}

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer)
{
    return AccessVolume(IRP_MJ_WRITE, Offset, Length, Buffer);
}

BOOLEAN
NtfsIsNameInExpression(_In_     PUNICODE_STRING Expression,
                       _In_     PUNICODE_STRING Name,
                       _In_     BOOLEAN IgnoreCase,
                       _In_opt_ PWCHAR UpcaseTable)
{
    return FsRtlIsNameInExpression(Expression,
                                   Name,
                                   IgnoreCase,
                                   UpcaseTable);
}

#ifdef __cplusplus
}
#endif
