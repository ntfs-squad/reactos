/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

NTSTATUS
FileRecord::CopyData(_In_    AttributeType Type,
                     _In_    PWSTR Name,
                     _In_    PUCHAR Buffer,
                     _Inout_ PULONG Length,
                     _In_    ULONGLONG Offset)
{
    PAttribute Attr = GetAttribute(Type, Name);

    if (Attr)
        return CopyData(Attr, Buffer, Length, Offset);

    return STATUS_NOT_FOUND;
}

NTSTATUS
FileRecord::CopyData(_In_    PAttribute Attr,
                     _In_    PUCHAR Buffer,
                     _Inout_ PULONG Length,
                     _In_    ULONGLONG Offset)
{
    NTSTATUS Status;
    ULONG BytesToRead, BytesRead, BytesInRun, DataPointer;
    PDataRun Head, CurrentDR;

    ASSERT(Buffer);
    ASSERT(Attr);

    if (*Length == 0)
    {
        // If we aren't reading anything, don't read anything.
        return STATUS_SUCCESS;
    }

    if (!(Attr->IsNonResident)) // Attribute is resident.
    {
        if (Offset >= Attr->Resident.DataLength)
        {
            // Don't read past the file data.
            DPRINT1("Offset is greater than or equal to the data size!\n");
            DPRINT1("Offset: %ld, Data Size: %ld\n", Offset, Attr->Resident.DataLength);
            return STATUS_END_OF_FILE;
        }

        // Determine number of bytes we need to copy.
        BytesToRead = min((Attr->Resident.DataLength - Offset), (*Length));

        // Copy attribute data into buffer.
        RtlCopyMemory(Buffer,
                      GetResidentDataPointer(Attr) + Offset,
                      BytesToRead);

        BytesRead = BytesToRead;
    }

    else // Attribute is nonresident.
    {
        if (Offset >= Attr->NonResident.DataSize)
        {
            // Don't read past the file data.
            DPRINT1("Offset >= DataSize! (%ld >= %ld)\n", Offset, Attr->NonResident.DataSize);
            return STATUS_END_OF_FILE;
        }

        // Determine number of bytes we need to copy.
        BytesToRead = min((Attr->NonResident.DataSize - Offset), (*Length));

        // Set up data runs
        Head = FindNonResidentData(Attr);
        CurrentDR = Head;
        BytesRead = 0;
        DataPointer = 0;

        while(CurrentDR)
        {
            // Get data run length
            BytesInRun = GetRunSize(CurrentDR);

            if (Offset >= BytesInRun)
            {
                Offset -= BytesInRun;
            }

            else
            {
                // We need to copy data from this run before moving to the next one.

                // Get data
                Status = Volume->ReadVolume(GetOffset(CurrentDR->LCN) + Offset,
                                            min(BytesToRead, (BytesInRun - Offset)),
                                            Buffer);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("Failed to read attribute contents!\n");
                    return Status;
                }

                // Adjust bytes read
                BytesRead += min(BytesToRead, (BytesInRun - Offset));

                // Are we done reading?
                if (BytesRead == BytesToRead)
                    break;

                // Clear offset
                if (Offset)
                    Offset = 0;
            }

            // Set up next data run
            CurrentDR = CurrentDR->NextRun;
        }

        // Free data run
        FreeDataRun(Head);

        // Check to make sure we read what was requested
        if (BytesRead != BytesToRead)
        {
            DPRINT1("Failed to copy file data!\n");
            return STATUS_NOT_FOUND;
        }
    }

    // Adjust length for caller
    *Length -= BytesRead;

    return STATUS_SUCCESS;
}