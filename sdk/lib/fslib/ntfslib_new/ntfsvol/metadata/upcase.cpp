/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"

NTSTATUS
Volume::UpcaseWideString(_Inout_ PWSTR WideString,
                         _In_    ULONG Length)
{
    NTSTATUS Status;
    PWSTR UpCaseBuffer;
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
    UpCaseBuffer = new(NonPagedPool) WCHAR[UpCaseDataLength / sizeof(WCHAR)];
    Status = UpCaseFile->CopyData(UpCaseData,
                                  (PUCHAR)UpCaseBuffer,
                                  &UpCaseDataLength);

    if (!NT_SUCCESS(Status))
        goto Done;

    // Update each character according to its entry in the $UpCase table
    for (int i = 0; i < Length; i++)
        WideString[i] = UpCaseBuffer[WideString[i]];

Done:
    delete UpCaseFile;
    delete UpCaseBuffer;
    return Status;
}
