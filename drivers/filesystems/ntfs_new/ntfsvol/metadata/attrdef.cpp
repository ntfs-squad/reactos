/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

NTSTATUS
NTFSVolume::GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                                     _Out_ AttributeType* Type)
{
    NTSTATUS Status;
    PFileRecord AttrDefFile;
    PAttribute DataAttr;
    PAttrDefEntry TableEntry;
    ULONG AttrDefEntryIndex, AttrDefDataSize, MaxIndex, NameCompareLength;
    PUCHAR Buffer = NULL;

    // NOTE: Lookup is case-insensitive.

    // Get file record for $AttrDef and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _AttrDef,
                                                       &AttrDefFile,
                                                       &DataAttr);

    // If this fails, there's something majorly wrong with this drive.
    if (!NT_SUCCESS(Status))
        return Status;

    Status = UpcaseWideString(AttributeTypeName, wcslen(AttributeTypeName));
    if (!NT_SUCCESS(Status))
        goto Done;

    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDefFile->CopyData(DataAttr,
                          Buffer,
                          &AttrDefDataSize,
                          0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    AttrDefEntryIndex = 0;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;
    NameCompareLength = wcslen(AttributeTypeName) * sizeof(WCHAR);

    for (int i = 0; i < MaxIndex; i++)
    {
        if ((wcslen(TableEntry->Label) * sizeof(WCHAR)) == NameCompareLength &&
            RtlCompareMemory(TableEntry->Label,
                             AttributeTypeName,
                             NameCompareLength) == NameCompareLength)
        {
            // We found the attribute name!
            *Type = AttributeType(TableEntry->AttributeType);
            Status = STATUS_SUCCESS;
            goto Done;
        }

        // Move onto the next element
        TableEntry++;
    }

    Status = STATUS_NOT_FOUND;

Done:
    delete AttrDefFile;
    if (Buffer)
        delete Buffer;
    return Status;
}