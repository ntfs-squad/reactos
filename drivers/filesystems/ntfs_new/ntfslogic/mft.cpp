#pragma once
#include <ntfsprocs.h>
#include "mft.h"


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
    __debugbreak();
    File->LoadData(FileRecordBuffer, FileRecordBufferLength);
    __debugbreak();
    return STATUS_SUCCESS;
}

/* *** FILE RECORD IMPLEMENTATIONS *** */
NTSTATUS
FileRecord::LoadData(PUCHAR FileRecordData, unsigned Length)
{
    AttrLength = Length - sizeof(FileRecordHeader);

    memcpy(&Header, &FileRecordData, sizeof(FileRecordHeader));

    memcpy(&AttrData,
           &FileRecordData[sizeof(FileRecordHeader)],
           AttrLength);

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
    IAttribute TempAttr;

    while (AttrDataPointer < AttrLength)
    {
        // Get Attribute Header
        memcpy(&TempAttr,
               &AttrData[AttrDataPointer],
               sizeof(IAttribute));

        if (TempAttr.AttributeType == Type)
        {
            // We found the right type of attribute!
            // Get name, if applicable.
            if (Name && TempAttr.NameLength)
            {
                memcpy(&Name,
                       &AttrData[AttrDataPointer + TempAttr.NameOffset],
                       TempAttr.NameLength);
            }

            // Figure out if resident or non-resident.
            if (TempAttr.NonResidentFlag)
            {
                // Non-resident. Return non-resident data type.
                Attr = new(NonPagedPool) NonResidentAttribute();
                memcpy(&Attr,
                       &AttrData[AttrDataPointer],
                       TempAttr.NameOffset);
                // No attribute data because the attribute is non-resident.
            }
            else
            {
                // Resident. Return resident data type.
                Attr = new(NonPagedPool) ResidentAttribute();
                // Get header
                memcpy(&Attr,
                       &AttrData[AttrDataPointer],
                       TempAttr.NameOffset);
                // Get data
                memcpy(&Data,
                       &AttrData[AttrDataPointer +
                                 ((ResidentAttribute*)Attr)->AttributeOffset],
                                 ((ResidentAttribute*)Attr)->AttributeLength);

            }
            return STATUS_SUCCESS;
        }
        else
        {
            // Wrong attribute, go to the next one.
            AttrDataPointer += TempAttr.Length;
        }
    }

    return STATUS_NOT_FOUND;
}