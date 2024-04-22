/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfs.h"

#include <ntdddisk.h>
#include <debug.h>

NTSTATUS
NtfsGlobalDriver::AreWeNtfs(PDEVICE_OBJECT DeviceToMount)
{
    DISK_GEOMETRY DiskGeometry;
    ULONG ClusterSize, Size, k;
    PBOOT_SECTOR BootSector;
    NTSTATUS Status;

    DPRINT("NtfsHasFileSystem() called\n");

    Size = sizeof(DISK_GEOMETRY);
    Status = NtfsBlockIo->DeviceIoControl(DeviceToMount,
                                          IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                          NULL,
                                          0,
                                          &DiskGeometry,
                                          &Size,
                                          TRUE);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("NtfsDeviceIoControl() failed (Status %lx)\n", Status);
        return Status;
    }

    DPRINT1("BytesPerSector: %lu\n", DiskGeometry.BytesPerSector);
    BootSector = (PBOOT_SECTOR)ExAllocatePoolWithTag(NonPagedPool,
                                       DiskGeometry.BytesPerSector,
                                       TAG_NTFS);
    if (BootSector == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = NtfsBlockIo->ReadBlock(DeviceToMount,
                                    0,
                                    1,
                                    DiskGeometry.BytesPerSector,
                                    (PUCHAR)BootSector,
                                    TRUE);
    if (!NT_SUCCESS(Status))
    {
        goto ByeBye;
    }

    /*
     * Check values of different fields. If those fields have not expected
     * values, we fail, to avoid mounting partitions that Windows won't mount.
     */

    /* OEMID: this field must be NTFS */
    if (RtlCompareMemory(BootSector->OEMID, "NTFS    ", 8) != 8)
    {
        DPRINT1("Failed with NTFS-identifier: [%.8s]\n", BootSector->OEMID);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto ByeBye;
    }

    /* Unused0: this field must be COMPLETELY null */
    for (k = 0; k < 7; k++)
    {
        if (BootSector->BPB.Unused0[k] != 0)
        {
            DPRINT1("Failed in field Unused0: [%.7s]\n", BootSector->BPB.Unused0);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto ByeBye;
        }
    }

    /* Unused3: this field must be COMPLETELY null */
    for (k = 0; k < 4; k++)
    {
        if (BootSector->BPB.Unused3[k] != 0)
        {
            DPRINT1("Failed in field Unused3: [%.4s]\n", BootSector->BPB.Unused3);
            Status = STATUS_UNRECOGNIZED_VOLUME;
            goto ByeBye;
        }
    }

    /* Check cluster size */
    ClusterSize = BootSector->BPB.BytesPerSector * BootSector->BPB.SectorsPerCluster;
    if (ClusterSize != 512 && ClusterSize != 1024 &&
        ClusterSize != 2048 && ClusterSize != 4096 &&
        ClusterSize != 8192 && ClusterSize != 16384 &&
        ClusterSize != 32768 && ClusterSize != 65536)
    {
        DPRINT1("Cluster size failed: %hu, %hu, %hu\n",
                BootSector->BPB.BytesPerSector,
                BootSector->BPB.SectorsPerCluster,
                ClusterSize);
        Status = STATUS_UNRECOGNIZED_VOLUME;
        goto ByeBye;
    }

ByeBye:
    ExFreePool(BootSector);

    return Status;
}

//NtfsBlockIo
NTSTATUS
NtfsGlobalDriver::MountVolume(_In_ PDEVICE_OBJECT DeviceObject,
                              _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION Stack;
    PDEVICE_OBJECT DeviceToMount;

    DPRINT("NtfsGlobalDriver::MountVolume() - called\n");
    if (DeviceObject != PubDeviceObject)
    {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        return Status;
    }

    /* First, Obtain the DeviceObject of the partition we're targetting. */
    Stack = IoGetCurrentIrpStackLocation(Irp);
    DeviceToMount = Stack->Parameters.MountVolume.DeviceObject;

    /* The function here returns, but it's not an error. we're a boot driver
     * NT will try every possible filesystem dr
     */
    Status = AreWeNtfs(DeviceToMount);
    DPRINT1("AreWeNtfs() returned %lx\n", Status);
    if (Status != STATUS_SUCCESS)
        return Status;

    Status = STATUS_UNRECOGNIZED_VOLUME;
    return Status;
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