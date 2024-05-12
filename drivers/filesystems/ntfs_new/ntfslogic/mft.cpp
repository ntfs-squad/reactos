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
FileRecord::FindFilenameAttribute(_In_ ResidentAttribute* Attr,
                                  _In_ FilenameAttr* ExtAttrHeader,
                                  _In_ PWSTR Filename)
{
    NTSTATUS Status;
    UCHAR Buffer[512];

    Status = FindAttribute(FileName, NULL, Attr, Buffer);

    if (Status == STATUS_SUCCESS)
    {
        RtlCopyMemory(ExtAttrHeader,
                      &Buffer,
                      sizeof(FilenameAttr));
        RtlCopyMemory(Filename,
                      &Buffer[sizeof(FilenameAttr)],
                      ExtAttrHeader->FilenameChars * sizeof(WCHAR));

        // Add null terminator to filename
        Filename[ExtAttrHeader->FilenameChars] = '\0';
    }

    return Status;
}

NTSTATUS
FileRecord::FindVolumenameAttribute(ResidentAttribute* Attr, PWSTR Data)
{
    NTSTATUS Status;

    Status = FindAttribute(VolumeName, NULL, Attr, (PUCHAR)Data);

    //Add null terminator
    if (Status == STATUS_SUCCESS)
        Data[Attr->AttributeLength / sizeof(WCHAR)] = '\0';

    return Status;
}

NTSTATUS
FileRecord::FindAttribute(AttributeType Type,
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