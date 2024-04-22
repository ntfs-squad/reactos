/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#include "ntfs.h"
#include <debug.h>

NtfsGlobalDriver::NtfsGlobalDriver(_In_ PDRIVER_OBJECT DriverObject,
                                   _In_ PDEVICE_OBJECT DeviceObject,
                                   _In_ PUNICODE_STRING RegistryPath)
{
    PubDriverObject = DriverObject;
    PubDeviceObject = DeviceObject;
    PubRegistryPath =  RegistryPath;

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