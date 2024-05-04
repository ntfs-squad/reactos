// #pragma once
// #include "globals.h"
// #include "mft.h"

// /* *** MFT IMPLEMENTATIONS *** */

// MFT::MFT(VolumeContextBlock* _In_ VolCB)
// {
//     VCB = VolCB;
// }

// NTSTATUS
// MFT::GetFileRecord(ULONGLONG FileRecordNumber,
//                    FileRecord* File)
// {
//     unsigned FileRecordBufferLength = VCB->ClustersPerFileRecord *
//                                       VCB->SectorsPerCluster *
//                                       VCB->BytesPerSector;

//     UCHAR FileRecordBuffer[1024]; //Hack, define FileRecordSizeMax

//     PubNtfsDriver->DumpBlocks(FileRecordBuffer,
//                               ((FileRecordNumber *
//                               VCB->ClustersPerFileRecord) +
//                               VCB->MFTLCN) *
//                               VCB->SectorsPerCluster,
//                               VCB->ClustersPerFileRecord *
//                               VCB->SectorsPerCluster,
//                               VCB->BytesPerSector);

//     File->LoadData(FileRecordBuffer, FileRecordBufferLength);

//     return STATUS_SUCCESS;
// }

// /* *** FILE RECORD IMPLEMENTATIONS *** */
// NTSTATUS
// FileRecord::LoadData(PUCHAR FileRecordData, unsigned Length)
// {
//     AttrLength = Length - sizeof(FileRecordHeader);

//     memcpy(&Header, &FileRecordData, sizeof(FileRecordHeader));

//     memcpy(&AttrData,
//            &FileRecordData[sizeof(FileRecordHeader)],
//            AttrLength);

//     return STATUS_SUCCESS;
// }

// NTSTATUS
// FileRecord::FindUnnamedAttribute(ULONG Type,
//                                  IAttribute* Attr,
//                                  PUCHAR Data)
// {
//     return FindNamedAttribute(Type, NULL, Attr, Data);
// }

// NTSTATUS
// FileRecord::FindNamedAttribute(ULONG Type,
//                                PCWSTR Name,
//                                IAttribute* Attr,
//                                PUCHAR Data)
// {
//     ULONG AttrDataPointer = 0;
//     IAttribute TempAttr;

//     while (AttrDataPointer < AttrLength)
//     {
//         // Get Attribute Header
//         memcpy(&TempAttr,
//                &AttrData[AttrDataPointer],
//                sizeof(IAttribute));

//         if (TempAttr.AttributeType == Type)
//         {
//             // We found the right type of attribute!
//             // Get name, if applicable.
//             if (Name && TempAttr.NameLength)
//             {
//                 memcpy(&Name,
//                        &AttrData[AttrDataPointer + TempAttr.NameOffset],
//                        TempAttr.NameLength);
//             }

//             // Figure out if resident or non-resident.
//             /*if (TempAttr.NonResidentFlag)
//             {
//                 // Non-resident. Return non-resident data type.
//                 Attr = new NonResidentAttribute();
//                 memcpy(&Attr,
//                        &AttrData[AttrDataPointer],
//                        TempAttr.NameOffset);
//                 // No attribute data because the attribute is non-resident.
//             }
//             else
//             {
//                 // Resident. Return resident data type.
//                 Attr = new ResidentAttribute();
//                 // Get header
//                 memcpy(&Attr,
//                        &AttrData[AttrDataPointer],
//                        TempAttr.NameOffset);
//                 // Get data
//                 memcpy(&Data,
//                        &AttrData[AttrDataPointer +
//                                  ((ResidentAttribute*)Attr)->AttributeOffset],
//                        ((ResidentAttribute*)Attr)->AttributeLength);

//             }*/
//             return STATUS_SUCCESS;
//         }
//         else
//         {
//             // Wrong attribute, go to the next one.
//             AttrDataPointer += TempAttr.Length;
//         }
//     }

//     return STATUS_NOT_FOUND;
// }