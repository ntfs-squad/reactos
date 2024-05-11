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
FileRecord::FindUnnamedAttribute(ULONG Type,
                                 IAttribute* Attr,
                                 PUCHAR Data)
{
    return FindNamedAttribute(Type, NULL, Attr, Data);
}

NTSTATUS
FileRecord::FindNamedAttribute(ULONG Type,
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
            DPRINT1("able to check attr type, correct!\n");
            // We found the right type of attribute!
            // Get name, if applicable.
            if (Name && Attr->NameLength)
            {
                RtlCopyMemory(&Name,
                              &AttrData[AttrDataPointer + Attr->NameOffset],
                              Attr->NameLength);
            }

            // Figure out if resident or non-resident.
            if (Attr->NonResidentFlag)
            {
                // Non-resident. Return non-resident data type.
                Attr = new(NonPagedPool) NonResidentAttribute();
                RtlCopyMemory(Attr,
                              &AttrData[AttrDataPointer],
                              Attr->NameOffset);
                // No attribute data because the attribute is non-resident.
            }
            else
            {
                // Resident. Return resident data type.
                Attr = new(NonPagedPool) ResidentAttribute();
                // Get header
                RtlCopyMemory(Attr,
                              &AttrData[AttrDataPointer],
                              Attr->NameOffset);
                // Get data
                RtlCopyMemory(Data,
                              &AttrData[AttrDataPointer +
                                       ((ResidentAttribute*)Attr)->AttributeOffset],
                              ((ResidentAttribute*)Attr)->AttributeLength);

            }
            return STATUS_SUCCESS;
        }
        else
        {
            // Wrong attribute, go to the next one.
            AttrDataPointer += Attr->Length;
            DPRINT1("Trying next attribute!\n");
        }
    }

    return STATUS_NOT_FOUND;
}