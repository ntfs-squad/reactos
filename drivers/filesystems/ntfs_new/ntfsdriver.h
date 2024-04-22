/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#pragma once

class NtfsGlobalDriver {
public:
    NtfsGlobalDriver(_In_ PDRIVER_OBJECT DriverObject,
                     _In_ PDEVICE_OBJECT DeviceObject,
                     _In_ PUNICODE_STRING RegistryPath);
    ~NtfsGlobalDriver();
    NTSTATUS FileSystemControl(_In_ PNTFS_IRP_CONTEXT IrpContext);
    NTSTATUS MountVolume(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
private:
    VOID NtfsGlobalDriver::CheckIfWeAreStupid(_In_ PUNICODE_STRING RegistryPath);   
    NTSTATUS NtfsGlobalDriver::AreWeNtfs(PDEVICE_OBJECT DeviceToMount);
public:
    PDRIVER_OBJECT PubDriverObject;
    PDEVICE_OBJECT PubDeviceObject;
    PUNICODE_STRING PubRegistryPath;
    BOOLEAN EnableWriteSupport;
    NtBlockIo* NtfsBlockIo;
};
