/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

INT
QueryDwordRegistryValue(_In_ PWCHAR Name,
                        _In_ INT Default)
{
    NTSTATUS Status;
    HANDLE hRegistryKey;
    UNICODE_STRING RegistryPath, ValueName;
    OBJECT_ATTRIBUTES Attributes;
    const UINT BufferSize = ROUND_UP(sizeof(KeyValuePartialInformation) + sizeof(ULONG), 0x10);
    UCHAR Buffer[BufferSize];
    ULONG DataLength;
    INT Result;

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
        return Default;
    }

    // Set up registry value
    RtlInitUnicodeString(&ValueName, Name);

    Status = ZwQueryValueKey(hRegistryKey,
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

    Result = *((INT*)(((PKEY_VALUE_PARTIAL_INFORMATION)Buffer)->Data));

    ZwClose(hRegistryKey);
    return Result;
}

BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name,
                          _In_ BOOLEAN Default)
{
    return !!QueryDwordRegistryValue(Name, Default ? 1 : 0);
}