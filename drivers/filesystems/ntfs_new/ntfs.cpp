/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

/* INCLUDES *****************************************************************/

#include "ntfs.h"

//#define NDEBUG
#include <debug.h>

/* GLOBALS ******************************************************************/

NtfsGlobalDriver* PubNtfsDriver;

// TODO: Remove old driver code here
typedef struct
{
    CACHE_MANAGER_CALLBACKS CacheMgrCallbacks;
    FAST_IO_DISPATCH FastIoDispatch;
    NPAGED_LOOKASIDE_LIST IrpContextLookasideList;
    NPAGED_LOOKASIDE_LIST FcbLookasideList;
    NPAGED_LOOKASIDE_LIST AttrCtxtLookasideList;
} NTFS_GLOBAL_DATA, *PNTFS_GLOBAL_DATA;

PNTFS_GLOBAL_DATA NtfsGlobalData = NULL;

/* FUNCTIONS ****************************************************************/

NTSTATUS
NTAPI
NtfsGenericDispatch(_In_ PDEVICE_OBJECT DeviceObject,
                    _In_ PIRP Irp)
{
    PNTFS_IRP_CONTEXT IrpContext = NULL;
    NTSTATUS Status;

    DPRINT("NtfsGenericDispatch()\n");


    IrpContext = (PNTFS_IRP_CONTEXT)ExAllocateFromNPagedLookasideList(&NtfsGlobalData->IrpContextLookasideList);
    if (IrpContext == NULL)
       return NULL;
    RtlZeroMemory(IrpContext, sizeof(NTFS_IRP_CONTEXT));
    IrpContext->Irp = Irp;
    IrpContext->DeviceObject = DeviceObject;
    IrpContext->Stack = IoGetCurrentIrpStackLocation(Irp);
    IrpContext->MajorFunction = IrpContext->Stack->MajorFunction;
    IrpContext->MinorFunction = IrpContext->Stack->MinorFunction;
    IrpContext->FileObject = IrpContext->Stack->FileObject;
    IrpContext->IsTopLevel = (IoGetTopLevelIrp() == Irp);
    IrpContext->Flags = IRPCONTEXT_COMPLETE;
    if (IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL ||
        IrpContext->MajorFunction == IRP_MJ_DEVICE_CONTROL ||
        IrpContext->MajorFunction == IRP_MJ_SHUTDOWN ||
        (IrpContext->MajorFunction != IRP_MJ_CLEANUP &&
         IrpContext->MajorFunction != IRP_MJ_CLOSE &&
         IoIsOperationSynchronous(Irp)))
    {
        IrpContext->Flags |= IRPCONTEXT_CANWAIT;
    }

    if (IrpContext == NULL)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    else
    {
        __debugbreak();
      //  Status = NtfsDispatch(IrpContext);
    }

    return Status;
}

EXTERN_C
CODE_SEG("INIT")
NTSTATUS
NTAPI
DriverEntry(_In_ PDRIVER_OBJECT DriverObject,
            _In_ PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(DEVICE_NAME);
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;

    DPRINT("DriverEntry(%p, '%wZ')\n", DriverObject, RegistryPath);

    Status = IoCreateDevice(DriverObject,
                            sizeof(PNTFS_GLOBAL_DATA), // We're not gonna use this yet.
                            &DeviceName,
                            FILE_DEVICE_DISK_FILE_SYSTEM,
                            0,
                            FALSE,
                            &DeviceObject);
    if (!NT_SUCCESS(Status))
    {
        DPRINT("IoCreateDevice failed with status: %lx\n", Status);
        return Status;
    }

    NtfsGlobalData = (PNTFS_GLOBAL_DATA)DeviceObject->DeviceExtension;
    PubNtfsDriver = new(PagedPool) NtfsGlobalDriver(DriverObject, DeviceObject, RegistryPath);
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                    = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                  = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_READ]                     = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_WRITE]                    = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]        = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]          = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION]   = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]        = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL]      = NtfsGenericDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]           = NtfsGenericDispatch;

    //These seem quite critical actually - but not required for ros?
    /* Initialize CC functions array */
    NtfsGlobalData->CacheMgrCallbacks.AcquireForLazyWrite = NtfsAcqLazyWrite;
    NtfsGlobalData->CacheMgrCallbacks.ReleaseFromLazyWrite = NtfsRelLazyWrite;
    NtfsGlobalData->CacheMgrCallbacks.AcquireForReadAhead = NtfsAcqReadAhead;
    NtfsGlobalData->CacheMgrCallbacks.ReleaseFromReadAhead = NtfsRelReadAhead;

    NtfsGlobalData->FastIoDispatch.SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);
    NtfsGlobalData->FastIoDispatch.FastIoCheckIfPossible = NtfsFastIoCheckIfPossible;
    NtfsGlobalData->FastIoDispatch.FastIoRead = NtfsFastIoRead;
    NtfsGlobalData->FastIoDispatch.FastIoWrite = NtfsFastIoWrite;
    DriverObject->FastIoDispatch = &NtfsGlobalData->FastIoDispatch;

    /* Initialize lookaside list for IRP contexts */
    ExInitializeNPagedLookasideList(&NtfsGlobalData->IrpContextLookasideList,
                                    NULL, NULL, 0, sizeof(NTFS_IRP_CONTEXT), TAG_IRP_CTXT, 0);
#if 0
    /* Initialize lookaside list for FCBs */
    ExInitializeNPagedLookasideList(&NtfsGlobalData->FcbLookasideList,
                                    NULL, NULL, 0, sizeof(NTFS_FCB), TAG_FCB, 0);
    /* Initialize lookaside list for attributes contexts */
    ExInitializeNPagedLookasideList(&NtfsGlobalData->AttrCtxtLookasideList,
                                    NULL, NULL, 0, sizeof(NTFS_ATTR_CONTEXT), TAG_ATT_CTXT, 0);
#endif

    /* Driver can't be unloaded */
    DriverObject->DriverUnload = NULL;

    DeviceObject->Flags |= DO_DIRECT_IO;

    /* Register file system */
    IoRegisterFileSystem(DeviceObject);
    ObReferenceObject(DeviceObject);


    DPRINT1("NTFS driver loaded\n");
    return STATUS_SUCCESS;
}
