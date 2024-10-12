#include "io/ntfsprocs.h"

NTSTATUS
FileRecord::CopyData(_In_ AttributeType Type,
                     _In_ PCWSTR Name,
                     _In_ PUCHAR Buffer,
                     _Inout_ PULONG Length)
{
    PAttribute Attr = GetAttribute(Type, Name);

    if (Attr)
        return CopyData(Attr, Buffer, Length);

    return STATUS_NOT_FOUND;
}

NTSTATUS
FileRecord::CopyData(_In_ PAttribute Attr,
                     _In_ PUCHAR Buffer,
                     _Inout_ PULONG Length)
{
    return CopyData(Attr, Buffer, 0, Length);
}

NTSTATUS
FileRecord::CopyData(_In_ PAttribute Attr,
                     _In_ PUCHAR Buffer,
                     _In_ ULONGLONG Offset,
                     _Inout_ PULONG Length)
{
    ULONG BytesToRead, BytesRead, BytesInRun, DataPointer;
    PDataRun Head, CurrentDR;

#ifdef NTFS_DEBUG
    ASSERT(Buffer);
    ASSERT(Attr);
#endif

    if (*Length == 0)
    {
        // If we aren't reading anything, don't read anything.
        return STATUS_SUCCESS;
    }

    if (!(Attr->IsNonResident))
    {
        if (Offset > Attr->Resident.DataLength)
        {
            // Don't read past the file data.
            DPRINT1("Offset is greater than the data size!\n");
            return STATUS_END_OF_FILE;
        }

        // Determine number of bytes we need to write.
        BytesToRead = min((Attr->Resident.DataLength), (*Length));

        DPRINT1("Bytes to read: %ld\n", BytesToRead);

        // Copy attribute data into buffer.
        RtlCopyMemory(Buffer,
                      GetResidentDataPointer(Attr) + Offset,
                      BytesToRead);

        // Adjust length for caller.
        *Length -= BytesToRead;

        return STATUS_SUCCESS;
    }

    else
    {
        // Attribute is nonresident.
        if (Offset > Attr->NonResident.DataSize)
        {
            // Don't read past the file data.
            DPRINT1("Offset is greater than the data size!\n");
            return STATUS_END_OF_FILE;
        }

        Head = FindNonResidentData(Attr);
        CurrentDR = Head;
        BytesRead = 0;
        DataPointer = 0;

        // We only support non-fragmented files for now.
        ASSERT(!(CurrentDR->NextRun));

        while(CurrentDR)
        {
            // Get data run length
            BytesInRun = (CurrentDR->Length) * Volume->BytesPerSector * Volume->SectorsPerCluster;

            // if (BytesInRun > *Length)
            //     BytesRead = *Length;
            // else
            //     BytesRead = BytesInRun;

            // Hack, let's just fill the buffer with the entire file contents.
            ASSERT(Attr->NonResident.DataSize == ALIGN_UP_BY(Attr->NonResident.DataSize, PAGE_SIZE));
            BytesRead = Attr->NonResident.DataSize;

            DPRINT1("Copying non-resident data...\n");
            DPRINT1("Buffer Length: %ld bytes.\n", *Length);
            DPRINT1("We chose to read: %ld bytes.\n", BytesRead);
            DPRINT1("Starting sector: %ld\n", (CurrentDR->LCN) * (Volume->SectorsPerCluster));
            DPRINT1("Sector size: 0x%X\n", Volume->BytesPerSector);

            // Get data
            LONGLONG Offset = (LONGLONG)((CurrentDR->LCN)*(Volume->SectorsPerCluster)) * (LONGLONG)(Volume->BytesPerSector);

            ReadDisk(Volume->PartDeviceObj, Offset, BytesRead, Buffer);

            DPRINT1("Buffer contents\n");
            for(int i = 0; i < 3; i++)
            {
                DPRINT1("Buffer[%ld]: %c\n", i, Buffer[i]);
            }

            // Set up next data run.
            CurrentDR = CurrentDR->NextRun;
        }

        *Length -= Attr->NonResident.DataSize;

done:
        // Free data run
        FreeDataRun(Head);

        return STATUS_SUCCESS;
    }
}