/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

NTSTATUS
Volume::GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                                 _Out_ AttributeType* Type)
{
    NTSTATUS Status;
    PFileRecord AttrDefFile;
    PAttribute DataAttr;
    PAttrDefEntry TableEntry;
    ULONG AttrDefDataSize, MaxIndex, NameLength, NameCompareLength;
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

    NameLength = wcslen(AttributeTypeName);
    Status = UpcaseWideString(AttributeTypeName, NameLength);
    if (!NT_SUCCESS(Status))
        goto Done;

    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDefFile->CopyData(DataAttr,
                          Buffer,
                          &AttrDefDataSize,
                          0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;
    NameCompareLength = NameLength * sizeof(WCHAR);

    for (ULONG i = 0; i < MaxIndex; i++)
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

/* Allocates a NUL-terminated copy of the first Length characters of Source. */
static PWSTR
CopyStreamName(_In_ PWSTR Source,
               _In_ ULONG Length)
{
    PWSTR StreamName = new(NonPagedPool) WCHAR[Length + 1];

    RtlCopyMemory(StreamName, Source, Length * sizeof(WCHAR));
    StreamName[Length] = L'\0';
    return StreamName;
}

NTSTATUS
Volume::GetADSPreference(_In_  PUNICODE_STRING FileName,
                         _Out_ AttributeType* RequestedType,
                         _Out_ PWSTR* RequestedStream)
{
    NTSTATUS Status;
    PWSTR FileNameQuery, ADSPtr, ADSTypePtr;

    ASSERT(FileName);

    FileNameQuery = FileName->Buffer;

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
                *RequestedStream = CopyStreamName(ADSPtr, ADSTypePtr - ADSPtr - 1);
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
            *RequestedStream = CopyStreamName(ADSPtr, wcslen(ADSPtr));

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