/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "ntfsprocs.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NtfsUnload)
#pragma alloc_text(PAGE, NtfsFsdCleanup)
#pragma alloc_text(PAGE, NtfsFsdLockControl)
#pragma alloc_text(PAGE, NtfsFsdDeviceControl)
#pragma alloc_text(PAGE, NtfsFsdShutdown)
#endif

PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;

#define TAG_IRP_CTXT 'iftN'
#define TAG_ATT_CTXT 'aftN'
#define TAG_FILE_REC 'rftN'
#define TAG_FCB 'FftN'

CACHE_MANAGER_CALLBACKS CacheMgrCallbacks;
FAST_IO_DISPATCH FastIoDispatch;
NPAGED_LOOKASIDE_LIST IrpContextLookasideList;
NPAGED_LOOKASIDE_LIST FcbLookasideList;
NPAGED_LOOKASIDE_LIST AttrCtxtLookasideList;
PDRIVER_OBJECT NtfsDriverObject;
/* FUNCTIONS ****************************************************************/

EXTERN_C
NTSTATUS
NTAPI
DriverEntry(_In_ PDRIVER_OBJECT DriverObject,
            _In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;
    UNICODE_STRING UnicodeString;
    NtfsDriverObject = DriverObject;
    UNREFERENCED_PARAMETER(RegistryPath);
    RtlInitUnicodeString(&UnicodeString, L"\\Ntfs");
    Status = IoCreateDevice(DriverObject,
                            0,
                            &UnicodeString,
                            FILE_DEVICE_DISK_FILE_SYSTEM,
                            0,
                            FALSE,
                            &NtfsDiskFileSystemDeviceObject);
    if (!NT_SUCCESS( Status )) {
        DPRINT("NtfsDriverEntry: Failed with Status %X\n", Status);
        return Status;
    }
    DriverObject->DriverUnload = NtfsUnload;

    DriverObject->MajorFunction[IRP_MJ_CREATE]                   = NtfsFsdCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                    = NtfsFsdClose;
    DriverObject->MajorFunction[IRP_MJ_READ]                     = NtfsFsdRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]                    = NtfsFsdWrite;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]        = NtfsFsdQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]          = NtfsFsdSetInformation;
    DriverObject->MajorFunction[IRP_MJ_QUERY_EA]                 = NtfsFsdQueryEa;
    DriverObject->MajorFunction[IRP_MJ_SET_EA]                   = NtfsFsdSetEa;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]            = NtfsFsdFlushBuffers;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = NtfsFsdQueryVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION]   = NtfsFsdSetVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                  = NtfsFsdCleanup;
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]        = NtfsFsdDirectoryControl;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL]      = NtfsFsdFileSystemControl;
    DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL]             = NtfsFsdLockControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]           = NtfsFsdDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]                 = NtfsFsdShutdown;
    //DriverObject->MajorFunction[IRP_MJ_PNP]                      = NtfsFsdPnp;

    NtfsDiskFileSystemDeviceObject->Flags |= DO_DIRECT_IO;


    /* Initialize lookaside list for IRP contexts */
    /*ExInitializeNPagedLookasideList(&IrpContextLookasideList,
                                    NULL, NULL, 0, sizeof(NTFS_IRP_CONTEXT), TAG_IRP_CTXT, 0);*/
        /* Initialize lookaside list for FCBs */
    /*ExInitializeNPagedLookasideList(&FcbLookasideList,
                                    NULL, NULL, 0, sizeof(NTFS_FCB), TAG_FCB, 0);*/
    /* Initialize lookaside list for attributes contexts */
    /*ExInitializeNPagedLookasideList(&AttrCtxtLookasideList,
                                    NULL, NULL, 0, sizeof(NTFS_ATTR_CONTEXT), TAG_ATT_CTXT, 0);*/
    /* Register file system */
    IoRegisterFileSystem(NtfsDiskFileSystemDeviceObject);
    ObReferenceObject(NtfsDiskFileSystemDeviceObject);

    return STATUS_SUCCESS;
}

_Function_class_(IRP_MJ_LOCK_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdLockControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                   _Inout_ PIRP Irp)
{
    DPRINT1("NtfsFsdLockControl: called\r\n");
    return 0;
}

_Function_class_(IRP_MJ_DEVICE_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDeviceControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp)
{
    IoSkipCurrentIrpStackLocation(Irp);
    __debugbreak();
    return 0;
}

_Function_class_(IRP_MJ_SHUTDOWN)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdShutdown (_In_ PDEVICE_OBJECT VolumeDeviceObject,
                 _Inout_ PIRP Irp)
{
    DPRINT1("NtfsFsdShutdown: called\r\n");
    return 0;
}

_Function_class_(DRIVER_UNLOAD)
EXTERN_C
VOID
NTAPI
NtfsUnload(_In_ _Unreferenced_parameter_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    ObDereferenceObject(NtfsDiskFileSystemDeviceObject);
}

_Function_class_(IRP_MJ_CLEANUP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCleanup(_In_ PDEVICE_OBJECT VolumeDeviceObject,
               _Inout_ PIRP Irp)
{
    DPRINT1("NtfsFsdCleanup: called\r\n");
    return 0;
}