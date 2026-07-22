/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

/* Reads the $UpCase table into the Volume cache. The table is immutable,
 * so this happens once; every later UpcaseWideString() call reuses it.
 */
NTSTATUS
Volume::LoadUpcaseTable()
{
    NTSTATUS Status;
    PFileRecord UpCaseFile;
    PAttribute UpCaseData;
    ULONG UpCaseDataLength;

    // Get $UpCase file
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _UpCase,
                                                       &UpCaseFile,
                                                       &UpCaseData);
    if (!NT_SUCCESS(Status))
        return Status;

    // Get $DATA attribute contents from $UpCase
    UpCaseDataLength = GetAttributeDataSize(UpCaseData);
    UpcaseTable = new(NonPagedPool) WCHAR[UpCaseDataLength / sizeof(WCHAR)];
    Status = UpCaseFile->CopyData(UpCaseData,
                                  (PUCHAR)UpcaseTable,
                                  &UpCaseDataLength);

    if (NT_SUCCESS(Status))
    {
        UpcaseTableLength = GetAttributeDataSize(UpCaseData) / sizeof(WCHAR);
    }
    else
    {
        delete[] UpcaseTable;
        UpcaseTable = NULL;
    }

    delete UpCaseFile;
    return Status;
}

NTSTATUS
Volume::UpcaseWideString(_Inout_ PWSTR WideString,
                         _In_    ULONG Length)
{
    NTSTATUS Status;

    if (!UpcaseTable)
    {
        Status = LoadUpcaseTable();
        if (!NT_SUCCESS(Status))
            return Status;
    }

    // Update each character according to its entry in the $UpCase table
    for (ULONG i = 0; i < Length; i++)
    {
        if (WideString[i] < UpcaseTableLength)
            WideString[i] = UpcaseTable[WideString[i]];
    }

    return STATUS_SUCCESS;
}
