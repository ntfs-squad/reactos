/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

#define ResidentAttributeHeaderSize 0xC0

#define CalculateNewRecordSize(OldAttribute, NewDataLength) \
((Header->ActualSize) - (OldAttribute->Resident.DataLength) + (NewDataLength))

#define MustPromoteAttribute(OldAttribute, NewDataLength) \
(CalculateNewRecordSize(OldAttribute, NewDataLength) > (Header->AllocatedSize))

#define CanDemoteAttribute(OldAttribute, NewDataLength) \
!MustPromoteAttribute(OldAttribute, NewDataLength)

#define MustBeNonResident(Length) \
((Header->ActualSize + Length + ResidentAttributeHeaderSize) > Header->AllocatedSize)

#define WriteToEOF(Offset) Offset == ~0

NTSTATUS
FileRecord::WriteFileData(_In_     AttributeType AttrType,
                          _In_opt_ PWSTR StreamName,
                          _In_     PUCHAR Buffer,
                          _Inout_  PULONG Length,
                          _In_     ULONGLONG Offset)
{
    NTSTATUS Status;

    DPRINT1("Called FileRecord::WriteData()!\n");

    Status = UpdateAttributeData(AttrType,
                                 StreamName,
                                 Buffer,
                                 *Length);

    // Write the file record to disk
    Status = MFT->WriteFileRecordToMFT(this);

    return Status;
}
