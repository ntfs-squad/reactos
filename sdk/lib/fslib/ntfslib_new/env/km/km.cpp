/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Kernelmode glue
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include <ntifs.h>
#include <ntfs_km.h>
#include <ntfslib_new_internal.h>

PDEVICE_OBJECT PartDeviceObj = NULL;
ULONG BytesPerSector = 0;

#ifdef __cplusplus
extern "C" {
#endif

void*
NtfsAllocatePoolWithTag(POOL_TYPE PoolType, size_t Size, ULONG Tag)
{
    return ExAllocatePoolWithTag(PoolType, Size, Tag);
}

void NtfsFreePool(void* pObject)
{
    ExFreePool(pObject);
}

NTSTATUS
NtfsQuerySystemTime(_Out_ PULONGLONG NtfsTime)
{
    LARGE_INTEGER Current;

    if (!NtfsTime)
        return STATUS_INVALID_PARAMETER;
    KeQuerySystemTime(&Current);
    if (Current.QuadPart < 0)
        return STATUS_INVALID_DEVICE_STATE;
    *NtfsTime = (ULONGLONG)Current.QuadPart;
    return STATUS_SUCCESS;
}

#ifdef __cplusplus
}
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG SectorBytes)
{
    if (!DeviceObject ||
        (SectorBytes != 512 && SectorBytes != 4096))
    {
        return STATUS_INVALID_PARAMETER;
    }

    PartDeviceObj = DeviceObject;
    BytesPerSector = SectorBytes;
    return STATUS_SUCCESS;
}

NTSTATUS
ReadDisk(_In_ PDEVICE_OBJECT DeviceObject,
         _In_ ULONGLONG Offset,
         _In_ ULONG Length,
         _Out_ PUCHAR Buffer)
{
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    PAGED_CODE();

    //  Initialize the event we're going to use
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    //  Build the IRP for the operation
    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                                       DeviceObject,
                                       Buffer,
                                       Length,
                                       (PLARGE_INTEGER) &Offset,
                                       &Event,
                                       &Iosb);

    if (!Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    SetFlag(IoGetNextIrpStackLocation(Irp)->Flags, SL_OVERRIDE_VERIFY_VOLUME);

    //  Call the device to do the read and wait for it to finish.
    Status = IoCallDriver(DeviceObject, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        Status = Iosb.Status;
    }

    NT_ASSERT(Status != STATUS_VERIFY_REQUIRED);

    /*  Special case this error code because this probably means we used
     *  the wrong sector size and we want to reject STATUS_WRONG_VOLUME.
     */
    if (Status == STATUS_INVALID_PARAMETER)
        return Status;

    //  If it doesn't succeed then either return or raise the error.
    if (!NT_SUCCESS(Status))
    {
        __debugbreak();
    }

    //  And return to our caller.
    return Status;
}

NTSTATUS
WriteDisk(_In_ PDEVICE_OBJECT DeviceObject,
          _In_ ULONGLONG Offset,
          _In_ ULONG Length,
          _In_ PUCHAR Buffer)
{
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    //This code can be paged, required for pretty much everything in this driver.
    PAGED_CODE();

    //  Initialize an event which will be used to STALL THE OS UNTIL THE OPERATION COMPLETES
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    /* let's build an IO request, the Irp Representing the request buffer. */
    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_WRITE, //we ARE writing
                                       DeviceObject, // this IS the devce
                                       Buffer, // This is the bufffer
                                       Length, /// how many bytes
                                       (PLARGE_INTEGER) &Offset, //offset on disk
                                       &Event, //event in question
                                       &Iosb); //status check

    if (!Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    SetFlag(IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME); // override this because it causes problems

    //  Call the device to do the write and wait for it to finish.
    Status = IoCallDriver(DeviceObject, Irp); // DO DE WRITE

    if (Status == STATUS_PENDING)
    {
        // Infinitely stall the OS until this kernel mode executive event completes
        (VOID)KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL);
        Status = Iosb.Status;
    }

    NT_ASSERT(Status != STATUS_VERIFY_REQUIRED);

    /*  Special case this error code because this probably means we used
     *  the wrong sector size and we want to reject STATUS_WRONG_VOLUME.
     */
    if (Status == STATUS_INVALID_PARAMETER)
        return Status;

    //  If it doesn't succeed then either return or raise the error.
    if (!NT_SUCCESS(Status))
        __debugbreak();

    //  And return to our caller.
    return Status;
}

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status;
    PUCHAR ReadBuffer;
    ULONGLONG SectorAlignedOffset;
    ULONG SectorAlignedLength;

    ASSERT(Length);

    SectorAlignedOffset = Offset - (Offset % BytesPerSector);
    SectorAlignedLength = ALIGN_UP_BY((Offset - SectorAlignedOffset) + Length,
                                      BytesPerSector);

    if (SectorAlignedOffset == Offset
        && SectorAlignedLength == Length)
    {
        // Read directly to the supplied buffer.
        Status = ReadDisk(PartDeviceObj,
                          SectorAlignedOffset,
                          SectorAlignedLength,
                          Buffer);
    }

    else
    {
        // Create the read buffer
        ReadBuffer = new(NonPagedPool) UCHAR[SectorAlignedLength];

        // Fill the read buffer.
        Status = ReadDisk(PartDeviceObj,
                          SectorAlignedOffset,
                          SectorAlignedLength,
                          ReadBuffer);

        if (NT_SUCCESS(Status))
        {
            // Copy the contents we need into the supplied buffer.
            RtlCopyMemory(Buffer,
                          ReadBuffer + (Offset % BytesPerSector),
                          Length);
        }

        // Free read buffer
        delete[] ReadBuffer;
    }

    return Status;
}

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status;
    PUCHAR WriteBuffer;
    ULONGLONG SectorAlignedOffset;
    ULONG SectorAlignedLength;

    SectorAlignedOffset = Offset - (Offset % BytesPerSector);
    SectorAlignedLength = ALIGN_UP_BY((Offset - SectorAlignedOffset) + Length,
                                      BytesPerSector);

    if (SectorAlignedOffset == Offset
        && SectorAlignedLength == Length)
    {
        // Write directly to the disk using the supplied buffer.
        Status = WriteDisk(PartDeviceObj,
                           SectorAlignedOffset,
                           SectorAlignedLength,
                           Buffer);
    }

    else
    {
        // Create the write buffer
        WriteBuffer = new(NonPagedPool) UCHAR[SectorAlignedLength];

        // Fill the write buffer with what's on disk.
        Status = ReadDisk(PartDeviceObj,
                          SectorAlignedOffset,
                          SectorAlignedLength,
                          WriteBuffer);

        if (NT_SUCCESS(Status))
        {
            // Copy the buffer contents we want to write into the write buffer.
            RtlCopyMemory(WriteBuffer + (Offset % BytesPerSector),
                          Buffer,
                          Length);

            // Write to the disk.
            Status = WriteDisk(PartDeviceObj,
                               SectorAlignedOffset,
                               SectorAlignedLength,
                               WriteBuffer);
        }

        // Free write buffer
        delete[] WriteBuffer;
    }

    return Status;
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
