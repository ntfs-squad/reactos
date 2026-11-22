/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

NTSTATUS
Volume::LoadUpcaseTable()
{
    NTSTATUS Status;
    PFileRecord UpCaseFile;
    PAttribute DataAttribute;
    PWCHAR Table = NULL;
    ULONG BytesToCopy = NUMBER_OF_UPCASE_ENTRIES * sizeof(WCHAR);

    // Find $UpCase file.
    Status = MFT->GetFileRecord(_UpCase, &UpCaseFile);
    if (!NT_SUCCESS(Status))
        return Status;

    // Get $DATA attribute.
    DataAttribute = UpCaseFile->GetAttribute(TypeData, NULL);
    if (!DataAttribute)
    {
        Status = STATUS_NOT_FOUND;
        goto Done;
    }

    // Verify $UpCase size.
    if ((GetAttributeDataSize(DataAttribute)) != BytesToCopy)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    // Allocate buffer for a copy of $UpCase in memory.
    Table = new(NtfsPagedPool) WCHAR[NUMBER_OF_UPCASE_ENTRIES];
    if (!Table)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    // Copy data from $UpCase into upcase table buffer.
    Status = UpCaseFile->CopyData(TypeData, NULL, reinterpret_cast<PUCHAR>(Table), &BytesToCopy);
    if (BytesToCopy != 0 && NT_SUCCESS(Status))
        Status = STATUS_FILE_CORRUPT_ERROR;

Done:
    delete UpCaseFile;
    if (NT_SUCCESS(Status))
        UpcaseTable = Table;
    else
        delete[] Table;
    return Status;
}

NTSTATUS
Volume::UpcaseWideString(_Inout_updates_(CharacterCount) PWSTR WideString,
                         _In_ ULONG CharacterCount)
{
    if (!NT_SUCCESS(UpcaseStatus))
        return UpcaseStatus;

    for (ULONG Index = 0; Index < CharacterCount; ++Index)
        WideString[Index] = UpcaseTable[WideString[Index]];

    return STATUS_SUCCESS;
}

NTSTATUS
Volume::UpcaseUnicodeString(_Inout_ PUNICODE_STRING String)
{
    return UpcaseWideString(String->Buffer, String->Length / sizeof(WCHAR));
}
