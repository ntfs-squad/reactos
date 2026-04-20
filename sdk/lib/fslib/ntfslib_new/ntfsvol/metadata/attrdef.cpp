/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"

NTSTATUS
Volume::GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
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

NTSTATUS
Volume::GetADSPreference(_In_  PFILE_OBJECT FileObj,
                         _Out_ AttributeType* RequestedType,
                         _Out_ PWSTR* RequestedStream)
{
    NTSTATUS Status;
    PWSTR FileNameQuery, ADSPtr, ADSTypePtr;
    ULONG StreamNameLength;

    ASSERT(FileObj);

    FileNameQuery = FileObj->FileName.Buffer;

    if (!FileNameQuery)
    {
        DPRINT1("FileNameQuery is NULL!\n");
        return STATUS_NOT_FOUND;
    }

    // Check for alternate data stream
    ADSPtr = wcschr(FileNameQuery, L':');

    if (ADSPtr)
    {
        /* This file request is for an alternate data stream.
         * Format:
         *     filename.ext:AttributeName:$AttributeType
         * If the last element is missing, it is equivalent to
         *     filename.ext:AttributeName:$DATA
         */

        // Go to the next character after the colon
        ADSPtr++;
        ADSTypePtr = wcschr(ADSPtr, L':');

        if (ADSTypePtr)
        {
            ADSTypePtr++;

            if (ADSPtr[0] == L':')
            {
                /* File requested is in this format:
                 *     filename.ext::$ATTRIBUTE_NAME
                 * Requested stream is NULL.
                 */
                RequestedStream = NULL;
            }

            else
            {
                // Copy the requested stream name.
                StreamNameLength = ADSTypePtr - ADSPtr - 1;
                *RequestedStream = new(NonPagedPool) WCHAR[StreamNameLength + 1];
                RtlCopyMemory(*RequestedStream,
                              ADSPtr,
                              StreamNameLength * sizeof(WCHAR));
                (*RequestedStream)[StreamNameLength] = L'\0';
            }

            // Stream name is copied, get the attribute type
            Status = GetAttributeTypeFromName(ADSTypePtr, RequestedType);

            if (!NT_SUCCESS(Status))
            {
                // If we fail to find the attribute type, the name was invalid.
                DPRINT1("Failed to find ADS attribute type!\n");
                delete *RequestedStream;
                return STATUS_OBJECT_NAME_INVALID;
            }
        }

        else
        {
            // Copy the stream name
            StreamNameLength = wcslen(ADSPtr);
            *RequestedStream = new(NonPagedPool) WCHAR[StreamNameLength + 1];
            RtlCopyMemory(*RequestedStream,
                          ADSPtr,
                          wcslen(ADSPtr) * sizeof(WCHAR));
            (*RequestedStream)[StreamNameLength] = L'\0';

            // No type specified, use $DATA
            *RequestedType = TypeData;
        }
    }

    else
    {
        // This is a normal file.
        *RequestedType = TypeData;
        *RequestedStream = NULL;
    }

    return STATUS_SUCCESS;
}