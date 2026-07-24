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
Volume::LoadAttributeDefinitions()
{
    NTSTATUS Status;
    PFileRecord AttrDefFile;
    PAttribute DataAttr;
    PAttrDefEntry TableEntry;
    ULONG AttrDefDataSize, EntryCount, MaxIndex;
    PUCHAR Buffer = NULL;

    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _AttrDef,
                                                       &AttrDefFile,
                                                       &DataAttr);

    if (!NT_SUCCESS(Status))
        return Status;

    Status = AttrDefFile->ReadAttributeAlloc(DataAttr,
                                             &Buffer,
                                             &AttrDefDataSize);
    if (!NT_SUCCESS(Status))
        goto Done;

    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;
    EntryCount = 0;
    while (EntryCount < MaxIndex &&
           TableEntry[EntryCount].AttributeType != 0)
    {
        ULONG LabelLength;
        for (LabelLength = 0;
             LabelLength < RTL_NUMBER_OF(TableEntry[EntryCount].Label) &&
             TableEntry[EntryCount].Label[LabelLength] != L'\0';
             LabelLength++)
        {
        }

        if (LabelLength == RTL_NUMBER_OF(TableEntry[EntryCount].Label))
        {
            Status = STATUS_FILE_CORRUPT_ERROR;
            goto Done;
        }
        EntryCount++;
    }

    if (EntryCount == 0)
    {
        Status = STATUS_FILE_CORRUPT_ERROR;
        goto Done;
    }

    AttrDefCache = new(PagedPool, TAG_NTFS) AttrDefCacheEntry[EntryCount];
    if (!AttrDefCache)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    AttrDefCacheCount = EntryCount;
    for (ULONG Index = 0; Index < EntryCount; Index++)
    {
        AttrDefCache[Index].Type =
            AttributeType(TableEntry[Index].AttributeType);
        RtlCopyMemory(AttrDefCache[Index].Label,
                      TableEntry[Index].Label,
                      sizeof(AttrDefCache[Index].Label));
    }
    Status = STATUS_SUCCESS;

Done:
    delete AttrDefFile;
    if (Buffer)
        delete[] Buffer;
    return Status;
}

NTSTATUS
Volume::GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                                 _Out_ AttributeType* Type)
{
    NTSTATUS Status;
    ULONG NameLength, NameCompareLength;

    if (!AttributeTypeName || !Type)
        return STATUS_INVALID_PARAMETER;

    if (!AttrDefCache)
    {
        Status = LoadAttributeDefinitions();
        if (!NT_SUCCESS(Status))
            return Status;
    }

    NameLength = NtfsWcsLen(AttributeTypeName);
    if (NameLength >= RTL_NUMBER_OF(AttrDefCache[0].Label))
        return STATUS_NOT_FOUND;

    Status = UpcaseWideString(AttributeTypeName, NameLength);
    if (!NT_SUCCESS(Status))
        return Status;

    NameCompareLength = NameLength * sizeof(WCHAR);
    for (ULONG Index = 0; Index < AttrDefCacheCount; Index++)
    {
        if (NtfsWcsLen(AttrDefCache[Index].Label) == NameLength &&
            RtlCompareMemory(AttrDefCache[Index].Label,
                             AttributeTypeName,
                             NameCompareLength) == NameCompareLength)
        {
            *Type = AttrDefCache[Index].Type;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
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

    if (!FileName || !RequestedType || !RequestedStream)
        return STATUS_INVALID_PARAMETER;

    *RequestedStream = NULL;

    FileNameQuery = FileName->Buffer;

    if (!FileNameQuery)
    {
        DPRINT1("FileNameQuery is NULL!\n");
        return STATUS_NOT_FOUND;
    }

    // Check for alternate data stream
    ADSPtr = NtfsWcsChr(FileNameQuery, L':');

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
        ADSTypePtr = NtfsWcsChr(ADSPtr, L':');

        if (ADSTypePtr)
        {
            ADSTypePtr++;

            if (ADSPtr[0] == L':')
            {
                /* File requested is in this format:
                 *     filename.ext::$ATTRIBUTE_NAME
                 * Requested stream is NULL.
                 */
                *RequestedStream = NULL;
            }

            else
            {
                // Copy the requested stream name.
                *RequestedStream = CopyStreamName(ADSPtr, ADSTypePtr - ADSPtr - 1);
                if (!*RequestedStream)
                    return STATUS_INSUFFICIENT_RESOURCES;
            }

            // Stream name is copied, get the attribute type
            Status = GetAttributeTypeFromName(ADSTypePtr, RequestedType);

            if (!NT_SUCCESS(Status))
            {
                // If we fail to find the attribute type, the name was invalid.
                DPRINT1("Failed to find ADS attribute type!\n");
                delete[] *RequestedStream;
                *RequestedStream = NULL;
                return STATUS_OBJECT_NAME_INVALID;
            }
        }

        else
        {
            // Copy the stream name
            *RequestedStream = CopyStreamName(ADSPtr, NtfsWcsLen(ADSPtr));
            if (!*RequestedStream)
                return STATUS_INSUFFICIENT_RESOURCES;

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
