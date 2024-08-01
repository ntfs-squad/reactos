#pragma once
#include "io/ntfsprocs.h"
#include <debug.h>
#include "mft.h"
#include "ntfsdbgprint.h"

/* *** MFT IMPLEMENTATIONS *** */

UCHAR FileRecordBuffer[0x100000];

MFT::MFT(_In_ PNtfsPartition ParentPartition)
{
    NtfsPart = ParentPartition;

    // Get MFT Sector Offset.
    MFTOffset = NtfsPart->MFTLCN * NtfsPart->SectorsPerCluster;

    /* Get File Record Size (Bytes).
     * If clusters per file record is less than 0, the file record size is 2^(-ClustersPerFileRecord).
     * Otherwise, the file record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector.
     */
    FileRecordSize = NtfsPart->ClustersPerFileRecord < 0 ?
                     1 << (-(NtfsPart->ClustersPerFileRecord)) :
                     NtfsPart->ClustersPerFileRecord * NtfsPart->SectorsPerCluster * NtfsPart->BytesPerSector;
}

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

PIAttribute
FileRecord::FindAttributePointer(_In_ AttributeType Type,
                                 _In_ PCWSTR Name)
{
    ULONG AttrDataPointer = 0;
    PIAttribute TestAttr;

    DPRINT1("Attribute Length: %d\n", AttrLength);

    while (AttrDataPointer < AttrLength)
    {
        // Test current attribute
        TestAttr = (PIAttribute)(&AttrData[AttrDataPointer]);
        PrintAttributeHeader(TestAttr);

        if (TestAttr->AttributeType == Type)
        {
            DPRINT1("Found attribute type!\n");

            // TODO: Search by attribute name.
            if (Name)
                continue;

            return (PIAttribute)&AttrData[AttrDataPointer];
        }

        else
        {
            // If the test attribute is 0 length, we are done.
            if (!TestAttr->Length)
                return NULL;

            AttrDataPointer += TestAttr->Length;

            DPRINT1("Trying next attribute!\n");
        }
    }

    return NULL;
}
