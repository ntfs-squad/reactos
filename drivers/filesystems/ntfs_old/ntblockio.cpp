#include "ntfs.h"
#include <debug.h>
//TODO: 

/*
 * There's a few problems with the orignal code here. (obviously this just a weak copy and paste)
 *    1. I don't like the override parameter
 *    2. SectorSize should be static or read from a different calss.
 *    3> DeviceObject needs to be handled differently?
 *    4. StartOffset as a name is annoyinh
 * 
 * This file will likely be rewritten. 
 */
NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceBeingRead,
         _In_    LONGLONG StartingOffset,
         _In_    ULONG AmountOfSectors,
         _In_    ULONG SectorSize,
         _Inout_ PUCHAR Buffer,
         _In_    BOOLEAN Override)
{
    KEVENT Event;
    PIRP Irp;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    PAGED_CODE();

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  Build the irp for the operation and also set the overrride flag
    //

    ByteOffset.QuadPart = StartingOffset;

    Irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                        DeviceBeingRead,
                                        Buffer,
                                        AmountOfSectors,
                                        &ByteOffset,
                                        &Event,
                                        &Iosb );

    if ( Irp == NULL ) {
        __debugbreak();
        //FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
    }

    SetFlag( IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME );

    //
    //  Call the device to do the read and wait for it to finish.
    //

    Status = IoCallDriver( DeviceBeingRead, Irp );

    if (Status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );

        Status = Iosb.Status;
    }

    NT_ASSERT( Status != STATUS_VERIFY_REQUIRED );

    //
    //  Special case this error code because this probably means we used
    //  the wrong sector size and we want to reject STATUS_WRONG_VOLUME.
    //

    if (Status == STATUS_INVALID_PARAMETER) {
        return Status;
    }

    //
    //  If it doesn't succeed then either return or raise the error.
    //

    if (!NT_SUCCESS(Status)) {
        __debugbreak();
    }

    //
    //  And return to our caller
    //

    return Status;
}

NTSTATUS
ReadBlock(_In_    PDEVICE_OBJECT DeviceObject,
                     _In_    ULONG DiskSector,
                     _In_    ULONG SectorCount,
                     _In_    ULONG SectorSize,
                     _Inout_ PUCHAR Buffer,
                     _In_    BOOLEAN Override)
{
    LONGLONG Offset;
    ULONG BlockSize;

    Offset = (LONGLONG)DiskSector * (LONGLONG)SectorSize;
    BlockSize = SectorCount * SectorSize;

    return ReadDisk(DeviceObject, Offset, BlockSize, SectorSize, Buffer, Override);
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