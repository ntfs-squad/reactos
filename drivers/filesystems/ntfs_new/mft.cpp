#pragma once
#include "io/ntfsprocs.h"
#include <debug.h>
#include "mft.h"
#include "ntfsdbgprint.h"

/* *** MFT IMPLEMENTATIONS *** */

MFT::MFT(_In_ PNtfsPartition ParentPartition)
{
    NtfsPart = ParentPartition;

    /* Get MFT Sector Offset. */
    MFTOffset = NtfsPart->MFTLCN * NtfsPart->SectorsPerCluster;

    /* Get File Record Size (Bytes). */
    if (NtfsPart->ClustersPerFileRecord > 0)
    {
        // File record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector
        FileRecordSize = NtfsPart->ClustersPerFileRecord * NtfsPart->SectorsPerCluster * NtfsPart->BytesPerSector;
    }

    else
    {
        // File record size is 2^(-ClustersPerFileRecord)
        FileRecordSize = 1 << (-(NtfsPart->ClustersPerFileRecord));
    }
}

UCHAR FileRecordBuffer[0x100000];

NTSTATUS
MFT::GetFileRecord(_In_  ULONGLONG FileRecordNumber,
                   _Out_ FileRecord* File)
{
    PAGED_CODE();

    INT FileRecordOffset;

    FileRecordOffset = (FileRecordNumber * FileRecordSize) / NtfsPart->BytesPerSector;

    NtfsPart->DumpBlocks(FileRecordBuffer,
                         MFTOffset + FileRecordOffset,
                         FileRecordSize / NtfsPart->BytesPerSector);

    File->LoadData(FileRecordBuffer,
                   FileRecordSize);
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
