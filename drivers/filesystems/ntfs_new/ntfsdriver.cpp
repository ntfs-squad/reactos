/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfs.h"
#include <debug.h>


NTSTATUS
NtfsGlobalDriver::MountVolume(_In_ PDEVICE_OBJECT DeviceObject,
                              _Inout_ PIRP Irp)
{
    DPRINT("NtfsGlobalDriver::MountVolume() - called\n");
    return STATUS_UNRECOGNIZED_VOLUME;
}

NTSTATUS
NtfsGlobalDriver::FileSystemControl(_In_ PNTFS_IRP_CONTEXT IrpContext)
{
    NTSTATUS Status;
    PIRP Irp;
    PDEVICE_OBJECT DeviceObject;

    DPRINT("NtfsGlobalDriver::FileSystemControl() - called\n");

    DeviceObject = IrpContext->DeviceObject;
    Irp = IrpContext->Irp;
    Irp->IoStatus.Information = 0;
    Status = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpContext->MinorFunction)
    {
        case IRP_MN_KERNEL_CALL:
            DPRINT("FileSystemControl: IRP_MN_USER_FS_REQUEST\n");
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;

        case IRP_MN_USER_FS_REQUEST:
            __debugbreak();
            //Status = NtfsUserFsRequest(DeviceObject, Irp);
            break;

        case IRP_MN_MOUNT_VOLUME:
            DPRINT("FileSystemControl: IRP_MN_MOUNT_VOLUME\n");
            Status = MountVolume(DeviceObject, Irp);
            break;

        case IRP_MN_VERIFY_VOLUME:
            __debugbreak();
            DPRINT("FileSystemControl: IRP_MN_VERIFY_VOLUME\n");
           // Status = NtfsVerifyVolume(DeviceObject, Irp);
            break;

        default:
            DPRINT("FileSystemControl: MinorFunction %d\n", IrpContext->MinorFunction);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    return Status;
}
 
NtfsGlobalDriver::NtfsGlobalDriver(_In_ PDRIVER_OBJECT DriverObject,
                                   _In_ PDEVICE_OBJECT DeviceObject,
                                   _In_ PUNICODE_STRING RegistryPath)
{
    PubDriverObject = DriverObject;
    PubDeviceObject = DeviceObject;
    PubRegistryPath =  RegistryPath;

    NtfsBlockIo = new(PagedPool) NtBlockIo();
    CheckIfWeAreStupid(RegistryPath);
}

NtfsGlobalDriver::~NtfsGlobalDriver()
{
    /* Get destroyed by parent */
}

VOID
NtfsGlobalDriver::CheckIfWeAreStupid(_In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES Attributes;
    HANDLE DriverKey = NULL;

    // Read registry to determine if write support should be enabled
    InitializeObjectAttributes(&Attributes,
                               RegistryPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&DriverKey, KEY_READ, &Attributes);
    if (NT_SUCCESS(Status))
    {
        UNICODE_STRING ValueName;
        UCHAR Buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
        PKEY_VALUE_PARTIAL_INFORMATION Value = (PKEY_VALUE_PARTIAL_INFORMATION)Buffer;
        ULONG ValueLength = sizeof(Buffer);
        ULONG ResultLength;

        RtlInitUnicodeString(&ValueName, L"MyDataDoesNotMatterSoEnableExperimentalWriteSupportForEveryNTFSVolume");

        Status = ZwQueryValueKey(DriverKey,
                                 &ValueName,
                                 KeyValuePartialInformation,
                                 Value,
                                 ValueLength,
                                 &ResultLength);

        if (NT_SUCCESS(Status) && Value->Data[0] == TRUE)
        {
            DPRINT1("\tEnabling write support - may the lord have mercy on your hard drive.\n");
            EnableWriteSupport = TRUE;
        }

        ZwClose(DriverKey);
    }
}