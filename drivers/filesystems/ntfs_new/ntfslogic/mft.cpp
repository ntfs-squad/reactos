#pragma once
#include <ntfsprocs.h>
#include <debug.h>
#include "mft.h"
#include "ntfsdbgprint.h"


/* *** MFT IMPLEMENTATIONS *** */

MFT::MFT(VolumeContextBlock* _In_ VolCB, PDEVICE_OBJECT DeviceObj)
{
    VCB = VolCB;
    PartDeviceObj = DeviceObj;
}

UCHAR FileRecordBuffer[0x100000];
NTSTATUS
MFT::GetFileRecord(ULONGLONG FileRecordNumber,
                   FileRecord* File)
{
    PAGED_CODE();
    unsigned FileRecordBufferLength = VCB->ClustersPerFileRecord *
                                     VCB->SectorsPerCluster *
                                     VCB->BytesPerSector;

    ReadBlock(PartDeviceObj,
              ((FileRecordNumber *
              VCB->ClustersPerFileRecord) +
              VCB->MFTLCN) *
              VCB->SectorsPerCluster,
              VCB->ClustersPerFileRecord *
              VCB->SectorsPerCluster,
              VCB->BytesPerSector,
              FileRecordBuffer,
              TRUE);

    File->LoadData(FileRecordBuffer, FileRecordBufferLength);
    return STATUS_SUCCESS;
}

/* *** FILE RECORD IMPLEMENTATIONS *** */

NTSTATUS
FileRecord::LoadData(PUCHAR FileRecordData, unsigned Length)
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

NTSTATUS
FileRecord::FindFilenameAttribute(FilenameAttr* Attr,
                                  WCHAR* Data)
{
    return FindAttribute(FileName, sizeof(FilenameAttr), NULL, Attr, (PUCHAR)Data);
}

NTSTATUS
FileRecord::FindAttribute(AttributeType Type,
                          ULONG HeaderSize,
                          PCWSTR Name,
                          IAttribute* Attr,
                          PUCHAR Data)
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

            // We found the right type of attribute!
            RtlCopyMemory(Attr,
                          &AttrData[AttrDataPointer],
                          HeaderSize);

            // Get name, if applicable.
            if (Name && Attr->NameLength)
            {
                RtlCopyMemory(&Name,
                              &AttrData[AttrDataPointer + Attr->NameOffset],
                              Attr->NameLength);
            }

            // Get attribute data if resident
            if (!Attr->NonResidentFlag)
            {
                RtlCopyMemory(Data,
                              &AttrData[AttrDataPointer + HeaderSize - 6], // HACK: Find a better way to do this
                              ((ResidentAttribute*)Attr)->AttributeLength);
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