#include "io/ntfsprocs.h"

#define GetOffset(LCN, Volume) (LCN * Volume->SectorsPerCluster * Volume->BytesPerSector)
#define GetRunSize(Run, Volume) (Run->Length * Volume->SectorsPerCluster * Volume->BytesPerSector)

NTSTATUS
FileRecord::CopyData(_In_    AttributeType Type,
                     _In_    PCWSTR Name,
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
            return STATUS_END_OF_FILE;
        }

        // Determine number of bytes we need to copy.
        BytesToRead = min((Attr->Resident.DataLength - Offset), (*Length));

        // Copy attribute data into buffer.
        RtlCopyMemory(Buffer,
                      GetResidentDataPointer(Attr) + Offset,
                      BytesToRead);
    }

    else // Attribute is nonresident.
    {
        if (Offset >= Attr->NonResident.DataSize)
        {
            // Don't read past the file data.
            DPRINT1("Offset is greater than or equal to the data size!\n");
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
            BytesInRun = GetRunSize(CurrentDR, Volume);

            if (Offset >= BytesInRun)
            {
                Offset -= BytesInRun;
            }

            else
            {
                // We need to copy data from this run before moving to the next one.

                // Get data
                ReadDiskUnaligned(Volume->PartDeviceObj,
                                  GetOffset(CurrentDR->LCN, Volume) + Offset,
                                  min(BytesToRead, (BytesInRun - Offset)),
                                  Buffer);

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
    }

    // Adjust length for caller
    *Length -= BytesToRead;

    return STATUS_SUCCESS;
}