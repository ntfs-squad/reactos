/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

// Macros for GetFreeClusters
const UINT8 Zeros[16] = { 4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0 };
#define GetZerosFromNibble(x) Zeros[(UINT8)x]
#define GetZerosFromByte(x) GetZerosFromNibble(x & 0xF) + GetZerosFromNibble(x >> 4)

NTSTATUS
NTFSVolume::UpcaseUnicodeString(_Inout_ PWSTR UnicodeString,
                                _In_    ULONG Length)
{
    // HACK: Uppercase the AttributeTypeName for case-insensitive matching
    for (int i = 0; i < Length; i++)
        UnicodeString[i] = RtlUpcaseUnicodeChar(UnicodeString[i]);

    return STATUS_SUCCESS;

    // Proper way is to use the $UpCase file
}

NTSTATUS
NTFSVolume::GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                                     _Out_ AttributeType* Type)
{
    NTSTATUS Status;
    PFileRecord AttrDefFile;
    PAttribute DataAttr;
    PAttrDefEntry TableEntry;
    ULONG AttrDefEntryIndex, AttrDefDataSize, MaxIndex, NameCompareLength;
    PUCHAR Buffer = NULL;

    // NOTE: Lookup is case-insensitive.

    // Get file record for $Bitmap and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _AttrDef,
                                                       &AttrDefFile,
                                                       &DataAttr);

    // If this fails, there's something majorly wrong with this drive.
    if (!NT_SUCCESS(Status))
        return Status;

    Status = UpcaseUnicodeString(AttributeTypeName, wcslen(AttributeTypeName));
    if (!NT_SUCCESS(Status))
        goto Done;

    AttrDefDataSize = DataAttr->NonResident.DataSize;
    Buffer = new(NonPagedPool) UCHAR[DataAttr->NonResident.DataSize];
    AttrDefFile->CopyData(DataAttr,
                          Buffer,
                          &AttrDefDataSize,
                          0);
    AttrDefDataSize = DataAttr->NonResident.DataSize - AttrDefDataSize;
    AttrDefEntryIndex = 0;
    MaxIndex = AttrDefDataSize / sizeof(AttrDefEntry);
    TableEntry = (PAttrDefEntry)Buffer;
    NameCompareLength = wcslen(AttributeTypeName) * sizeof(WCHAR);

    for (int i = 0; i < MaxIndex; i++)
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

NTSTATUS
NTFSVolume::GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters)
{
    // Note: $Bitmap is *always* non-resident on Windows.
    NTSTATUS Status;
    PFileRecord BitmapFile;
    PAttribute BitmapData;
    ULONG BytesToRead;
    PUCHAR BitmapBuffer;

    // Get file record for $Bitmap and $DATA attribute.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeData,
                                                       NULL,
                                                       _Bitmap,
                                                       &BitmapFile,
                                                       &BitmapData);

    if (!NT_SUCCESS(Status))
        return Status;

    // Get the size of $Bitmap
    BytesToRead = GetAttributeDataSize(BitmapData);

    // Initialize bitmap buffer
    BitmapBuffer = new(NonPagedPool) UCHAR[BytesToRead];

    // Copy attribute data into this buffer.
    BitmapFile->CopyData(BitmapData,
                         BitmapBuffer,
                         &BytesToRead);

    BytesToRead = GetAttributeDataSize(BitmapData) - BytesToRead;
    FreeClusters->QuadPart = 0;

    for (int i = 0; i < BytesToRead; i++)
        FreeClusters->QuadPart += GetZerosFromByte(BitmapBuffer[i]);

    Status = STATUS_SUCCESS;

    // We're done! Time to cleanup.
    delete BitmapBuffer;
    delete BitmapFile;
    return Status;
}

NTSTATUS
NTFSVolume::GetVolumeLabel(_Inout_ PWSTR VolumeLabel,
                           _Inout_ PUSHORT Length)
{
    NTSTATUS Status;
    PFileRecord VolumeFile;
    PAttribute VolumeNameAttr;
    UINT32 AttrLength;

    // Allocate memory for $Volume file record and retrieve the file record.
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeName,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeNameAttr);
    if (!NT_SUCCESS(Status))
        return Status;

    AttrLength = VolumeNameAttr->Resident.DataLength;

    // Copy volume name into VolumeLabel.
    RtlCopyMemory(VolumeLabel,
                  GetResidentDataPointer(VolumeNameAttr),
                  AttrLength);

    // Add null-terminator.
    VolumeLabel[AttrLength / sizeof(WCHAR)] = '\0';

    // Set length to attribute length.
    *Length = AttrLength;

    if (VolumeFile)
        delete VolumeFile;
    return Status;
}

NTSTATUS
NTFSVolume::SetVolumeLabel(_In_ PWSTR VolumeLabel,
                           _In_ ULONG Length)
{
    NTSTATUS Status;
    FileRecord* VolumeFile;
    PAttribute VolumeNameAttr;

    /* Allocate memory for $Volume file record, retrieve the file record, and
     * get a pointer to the $VOLUME_NAME attribute.
     */
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeName,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeNameAttr);

    if (!NT_SUCCESS(Status))
        return Status;

    // Update the resident data attribute for volume name
    Status = VolumeFile->UpdateResidentData(VolumeNameAttr,
                                            (PUCHAR)VolumeLabel,
                                            &Length);

    if (!NT_SUCCESS(Status))
        goto Done;

    // Write the volume file to disk.
    Status = MFT->WriteFileRecordToMFT(VolumeFile);

    if (!NT_SUCCESS(Status))
        goto Done;

Done:
    delete VolumeFile;
    return Status;
}