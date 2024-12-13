/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

HANDLE
OpenRegistryKey()
{
    NTSTATUS Status;
    HANDLE hRegistryKey;
    UNICODE_STRING RegistryPath;
    OBJECT_ATTRIBUTES Attributes;

    // Set up registry path
    RtlInitUnicodeString(&RegistryPath,
                         L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\FileSystem");

    InitializeObjectAttributes(&Attributes,
                               &RegistryPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&hRegistryKey, KEY_READ, &Attributes);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to open registry key!\n");
        return NULL;
    }

    // Caller must close this registry key.
    return hRegistryKey;
}

INT
QueryDwordRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ INT Default)
{
    NTSTATUS Status;
    UNICODE_STRING ValueName;
    const UINT BufferSize = ROUND_UP(sizeof(KeyValuePartialInformation) + sizeof(ULONG), 0x10);
    UCHAR Buffer[BufferSize];
    ULONG DataLength;

    if (!RegistryKey)
        return Default;

    // Set up registry value
    RtlInitUnicodeString(&ValueName, Name);

    Status = ZwQueryValueKey(RegistryKey,
                             &ValueName,
                             KeyValuePartialInformation,
                             Buffer,
                             BufferSize,
                             &DataLength);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to query registry value \"%S\"!\n", Name);
        return Default;
    }

    return *((INT*)(((PKEY_VALUE_PARTIAL_INFORMATION)Buffer)->Data));
}

INT
QueryDwordRegistryValue(_In_ PWCHAR Name,
                        _In_ INT Default)
{
    HANDLE RegistryKey;

    RegistryKey = OpenRegistryKey();
    if (!RegistryKey)
        return Default;
    return QueryDwordRegistryValue(RegistryKey, Name, Default);
}

BOOLEAN
QueryBooleanRegistryValue(_In_ HANDLE RegistryKey,
                          _In_ PWCHAR Name,
                          _In_ BOOLEAN Default)
{
    return !!QueryDwordRegistryValue(RegistryKey, Name, Default ? 1 : 0);
}

BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name,
                          _In_ BOOLEAN Default)
{
    return !!QueryDwordRegistryValue(Name, Default ? 1 : 0);
}

NTSTATUS
SetDwordRegistryValue(_In_ HANDLE RegistryKey,
                      _In_ PWCHAR Name,
                      _In_ INT Data)
{
    UNICODE_STRING ValueName;

    if (!RegistryKey)
        return STATUS_INVALID_PARAMETER;

    // Set up registry value
    RtlInitUnicodeString(&ValueName, Name);

    return ZwSetValueKey(RegistryKey,
                         &ValueName,
                         0,
                         REG_DWORD,
                         &Data,
                         sizeof(INT));
}

NTSTATUS
SetBooleanRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ BOOLEAN Data)
{
    return SetDwordRegistryValue(RegistryKey, Name, Data ? 1 : 0);
}