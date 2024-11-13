#include "ntfspch.h"

BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name)
{
    NTSTATUS Status;
    HANDLE hRegistryKey;
    UNICODE_STRING RegistryPath, ValueName;
    OBJECT_ATTRIBUTES Attributes;
    const UINT BufferSize = ROUND_UP(sizeof(KeyValuePartialInformation) + sizeof(ULONG), 0x10);
    UCHAR Buffer[BufferSize];
    ULONG DataLength;
    BOOLEAN Result;

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
        return FALSE;
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
        return FALSE;
    }

    Result = FALSE;

    for (int i = 0; i < sizeof(DWORD); i++)
    {
        if (((PKEY_VALUE_PARTIAL_INFORMATION)Buffer)->Data[i])
        {
            Result = TRUE;
            break;
        }
    }

    ZwClose(hRegistryKey);
    return Result;
}
