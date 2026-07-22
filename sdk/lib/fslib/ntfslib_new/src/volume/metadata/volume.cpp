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
Volume::GetVolumeLabel(_Inout_ PWSTR   VolumeLabel,
                       _Inout_ PUSHORT Length)
{
    NTSTATUS Status;
    PFileRecord VolumeFile;
    PAttribute VolumeNameAttr;
    ULONG LabelLength;

    // Retrieve the $Volume file record and $VOLUME_NAME attribute
    Status = MFT->GetFileAttributeFromFileRecordNumber(TypeVolumeName,
                                                       NULL,
                                                       _Volume,
                                                       &VolumeFile,
                                                       &VolumeNameAttr);
    if (!NT_SUCCESS(Status))
        return Status;

    LabelLength = GetAttributeDataSize(VolumeNameAttr);
    *Length = LabelLength;

    // Get the data from the attribute
    Status = VolumeFile->CopyData(VolumeNameAttr,
                                  (PUCHAR)VolumeLabel,
                                  &LabelLength);

    if (!NT_SUCCESS(Status) || LabelLength != 0)
        *Length = 0;

    delete VolumeFile;
    return Status;
}

NTSTATUS
Volume::SetVolumeLabel(_In_ PWSTR VolumeLabel,
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

    if (NT_SUCCESS(Status))
    {
        // Write the volume file to disk.
        Status = MFT->WriteFileRecordToMFT(VolumeFile);
    }

    delete VolumeFile;
    return Status;
}
