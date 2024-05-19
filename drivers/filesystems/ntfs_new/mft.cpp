#pragma once
#include "io/ntfsprocs.h"
#include <debug.h>
#include "mft.h"
#include "ntfsdbgprint.h"


/* *** MFT IMPLEMENTATIONS *** */

MFT::MFT(_In_ PNtfsPartition ParentPartition)
{
    NtfsPart = ParentPartition;
}

UCHAR FileRecordBuffer[0x100000];

NTSTATUS
MFT::GetFileRecord(_In_  ULONGLONG FileRecordNumber,
                   _Out_ FileRecord* File)
{
    PAGED_CODE();
    NtfsPart->DumpBlocks(FileRecordBuffer,
                         ((FileRecordNumber *
                           NtfsPart->ClustersPerFileRecord) +
                          NtfsPart->MFTLCN) *
                         NtfsPart->SectorsPerCluster,
                         NtfsPart->ClustersPerFileRecord *
                         NtfsPart->SectorsPerCluster);

    File->LoadData(FileRecordBuffer,
                   NtfsPart->ClustersPerFileRecord *
                   NtfsPart->SectorsPerCluster *
                   NtfsPart->BytesPerSector);
    return STATUS_SUCCESS;
}

/* *** FILE RECORD IMPLEMENTATIONS *** */

NTSTATUS
FileRecord::LoadData(_In_ PUCHAR FileRecordData,
                     _In_ unsigned Length)
{
    PAGED_CODE();
    AttrLength = Length - sizeof(FileRecordHeader);

    RtlCopyMemory(&Header, &FileRecordData, sizeof(FileRecordHeader));
    RtlCopyMemory(&AttrData,
                  &FileRecordData[Header->AttributeOffset],
                  AttrLength);

    PrintFileRecordHeader(Header);
    return STATUS_SUCCESS;
}

/* Find Attribute Functions */

NTSTATUS
FileRecord::FindAttribute(_In_ AttributeType Type,
                          _Out_ PCWSTR Name,
                          _Out_ IAttribute* Attr,
                          _Out_ PUCHAR Data)
{
    ULONG AttrDataPointer = 0;

    DPRINT1("Attribute Length: %d\n", AttrLength);

    while (AttrDataPointer < AttrLength)
    {
        // Get Attribute Header
        RtlCopyMemory(Attr,
                      &AttrData[AttrDataPointer],
                      sizeof(IAttribute));

        PrintAttributeHeader(Attr);

        if (Attr->AttributeType == Type)
        {
            DPRINT1("Found attribute!\n");

            // Copy to correct attribute header
            // Check if Non Resident
            if (Attr->NonResidentFlag)
            {
                RtlCopyMemory(Attr,
                              &AttrData[AttrDataPointer],
                              sizeof(NonResidentAttribute));
            }

            // File is resident
            else
            {
                RtlCopyMemory(Attr,
                              &AttrData[AttrDataPointer],
                              sizeof(ResidentAttribute));

                 // Get attribute data if resident and data is not null
                if (Data)
                {
                    RtlCopyMemory(Data,
                                  &AttrData[AttrDataPointer + ((ResidentAttribute*)Attr)->AttributeOffset],
                                  ((ResidentAttribute*)Attr)->AttributeLength);
                }
            }

            // Get name, if applicable.
            if (Name && Attr->NameLength)
            {
                RtlCopyMemory(&Name,
                              &AttrData[AttrDataPointer + Attr->NameOffset],
                              Attr->NameLength);
            }

            return STATUS_SUCCESS;
        }

        else
        {
            // Wrong attribute, go to the next one.
            if (!Attr->Length)
                return STATUS_UNSUCCESSFUL;

            AttrDataPointer += Attr->Length;
            DPRINT1("Trying next attribute!\n");
        }
    }

    return STATUS_NOT_FOUND;
}

NTSTATUS
FileRecord::FindFileNameAttribute(_Out_ ResidentAttribute* Attr,
                                  _Out_ FileNameEx* AttrHeaderEx,
                                  _Out_ PWSTR Filename)
{
    NTSTATUS Status;
    UCHAR Buffer[512];

    Status = FindAttribute(FileName, NULL, Attr, Buffer);

    if (Status == STATUS_SUCCESS)
    {
        RtlCopyMemory(AttrHeaderEx,
                      &Buffer,
                      sizeof(FileNameEx));
        RtlCopyMemory(Filename,
                      &Buffer[sizeof(FileNameEx)],
                      AttrHeaderEx->FilenameChars * sizeof(WCHAR));

        // Add null terminator to filename
        Filename[AttrHeaderEx->FilenameChars] = '\0';
    }

    return Status;
}

NTSTATUS
FileRecord::FindVolumeNameAttribute(_Out_ ResidentAttribute* Attr,
                                    _Out_ PWSTR Data)
{
    NTSTATUS Status;

    Status = FindAttribute(VolumeName, NULL, Attr, (PUCHAR)Data);

    //Add null terminator
    if (Status == STATUS_SUCCESS)
        Data[Attr->AttributeLength / sizeof(WCHAR)] = '\0';

    return Status;
}

NTSTATUS
FileRecord::FindStandardInformationAttribute(_Out_ ResidentAttribute* Attr,
                                             _Out_ StandardInformationEx* AttrHeaderEx)
{
    return FindAttribute(StandardInformation, NULL, Attr, (PUCHAR)AttrHeaderEx);
}