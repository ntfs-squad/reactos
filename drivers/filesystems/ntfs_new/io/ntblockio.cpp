#include "ntfsprocs.h"

NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceToRead,
         _In_    LONGLONG Offset,
         _In_    ULONG Length,
         _Inout_ PUCHAR Buffer)
{
    KEVENT Event;
    PIRP Irp;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    PAGED_CODE();

    //  Initialize the event we're going to use
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    //  Build the irp for the operation
    ByteOffset.QuadPart = Offset;
    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                                       DeviceToRead,
                                       Buffer,
                                       Length,
                                       &ByteOffset,
                                       &Event,
                                       &Iosb);

    if (Irp == NULL)
    {
        __debugbreak();
    }

    SetFlag(IoGetNextIrpStackLocation(Irp)->Flags, SL_OVERRIDE_VERIFY_VOLUME);

    //  Call the device to do the read and wait for it to finish.
    Status = IoCallDriver(DeviceToRead, Irp);

    if (Status == STATUS_PENDING)
    {
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
    {
        __debugbreak();
    }

    //  And return to our caller.
    return Status;
}

NTSTATUS
ReadDiskUnaligned(_In_     PDEVICE_OBJECT DeviceToRead,
                   _In_    LONGLONG Offset,
                   _In_    ULONG Length,
                   _Inout_ PUCHAR Buffer)
{
    NTSTATUS Status;
    PUCHAR PageAlignmentBuffer = NULL;
    USHORT RaggedEdgeSize = 0;
    USHORT SectorSize;

    ASSERT(Length);
    SectorSize = DeviceToRead->SectorSize;

    if (ALIGN_UP_BY(Length, SectorSize) != Length)
    {
        DPRINT1("ALIGN_UP_BY(Length, SectorSize) != Length\n");
        DPRINT1("%ld != %ld\n", ALIGN_UP_BY(Length, SectorSize), Length);
        RaggedEdgeSize = ALIGN_UP_BY(Length, SectorSize) - Length;
        PageAlignmentBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, RaggedEdgeSize, TAG_NTFS);
        RtlCopyMemory(PageAlignmentBuffer,
                      Buffer + Length,
                      RaggedEdgeSize);
    }

    Status = ReadDisk(DeviceToRead,
                      Offset,
                      ALIGN_UP_BY(Length, SectorSize),
                      Buffer);

    if (PageAlignmentBuffer)
    {
        // Copy data back where we overwrote it
        RtlCopyMemory(Buffer + Length,
                      PageAlignmentBuffer,
                      RaggedEdgeSize);

        // Free page alignment buffer
        delete PageAlignmentBuffer;
    }

    return Status;
}

// You might notice Carl this looks exactly like ReadDisk, So let's go over WHY...
NTSTATUS
WriteDisk(_In_    PDEVICE_OBJECT DeviceBeingRead,
          _In_    LONGLONG StartingOffset,
          _In_    ULONG AmountOfBytes,
          _In_    PUCHAR BufferToWrite)
{
    KEVENT Event;
    PIRP Irp;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    //This code can be paged, required for pretty much everything in this driver.
    PAGED_CODE();

    //  Initialize an event which will be used to STALL THE OS UNTIL THE OPERATION COMPLETES
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    //  Convert the offset into a LARGE_INTEGER
    ByteOffset.QuadPart = StartingOffset;

    /* let's build an IO request, the Irp Representing the request buffer. */
    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_WRITE, //we ARE writing
                                       DeviceBeingRead, // this IS the devce
                                       BufferToWrite, // This is the bufffer
                                       AmountOfBytes, /// how many bytes
                                       &ByteOffset, //offset on disk
                                       &Event, //event in question
                                       &Iosb); //status check

    if (Irp == NULL) //if an IO request cant be allocated
    {
        __debugbreak(); // LOL LMAO
        //FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
    }

    SetFlag(IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME); // override this because it causes problems

    //  Call the device to do the write and wait for it to finish.
    Status = IoCallDriver(DeviceBeingRead, Irp); // DO DE WRITE

    if (Status == STATUS_PENDING)
    {
        // Infinitely stall the OS until this kernel mode executive event completes
        (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );
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
ReadBlock(_In_    PDEVICE_OBJECT DeviceObject,
          _In_    ULONG DiskSector,
          _In_    ULONG SectorCount,
          _In_    ULONG SectorSize,
          _Inout_ PUCHAR Buffer)
{
    LONGLONG Offset;
    ULONG BlockSize;

    Offset = (LONGLONG)DiskSector * (LONGLONG)SectorSize;
    BlockSize = SectorCount * SectorSize;

    return ReadDisk(DeviceObject, Offset, BlockSize, Buffer);
}

NTSTATUS
WriteBlock(_In_    PDEVICE_OBJECT DeviceObject,
           _In_    ULONG DiskSector,
           _In_    ULONG SectorCount,
           _In_    ULONG SectorSize,
           _Inout_ PUCHAR Buffer)
{
    LONGLONG Offset;
    ULONG BlockSize;

    Offset = (LONGLONG)DiskSector * (LONGLONG)SectorSize;
    BlockSize = SectorCount * SectorSize; // NUMBER OF BLOCKS

    return WriteDisk(DeviceObject, Offset, BlockSize, Buffer );
}


NTSTATUS
DeviceIoControl(_In_    PDEVICE_OBJECT DeviceObject,
                _In_    ULONG ControlCode,
                _In_    PVOID InputBuffer,
                _In_    ULONG InputBufferSize,
                _Inout_ PVOID OutputBuffer,
                _Inout_ PULONG OutputBufferSize,
                _In_    BOOLEAN Override)
{
    PIO_STACK_LOCATION Stack;
    IO_STATUS_BLOCK IoStatus;
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    DPRINT("Building device I/O control request ...\n");
    Irp = IoBuildDeviceIoControlRequest(ControlCode,
                                        DeviceObject,
                                        InputBuffer,
                                        InputBufferSize,
                                        OutputBuffer,
                                        (OutputBufferSize) ? *OutputBufferSize : 0,
                                        FALSE,
                                        &Event,
                                        &IoStatus);
    if (Irp == NULL)
    {
        DPRINT("IoBuildDeviceIoControlRequest() failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (Override)
    {
        Stack = IoGetNextIrpStackLocation(Irp);
        Stack->Flags |= SL_OVERRIDE_VERIFY_VOLUME;
    }

    DPRINT("Calling IO Driver... with irp %p\n", Irp);
    Status = IoCallDriver(DeviceObject, Irp);
    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
        Status = IoStatus.Status;
    }

    if (OutputBufferSize)
    {
        *OutputBufferSize = IoStatus.Information;
    }

    return Status;
}