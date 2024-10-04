#include "io/ntfsprocs.h"

NTSTATUS
FileRecord::CopyData(_In_ AttributeType Type,
                     _In_ PCWSTR Name,
                     _In_ PUCHAR Buffer,
                     _Inout_ PULONGLONG Length)
{
    PAttribute Attr = GetAttribute(Type, Name);

    if (Attr)
        return CopyData(Attr, Buffer, Length);

    return STATUS_NOT_FOUND;
}

NTSTATUS
FileRecord::CopyData(_In_ PAttribute Attr,
                     _In_ PUCHAR Buffer,
                     _Inout_ PULONGLONG Length)
{
    return CopyData(Attr, Buffer, 0, Length);
}

NTSTATUS
FileRecord::CopyData(_In_ PAttribute Attr,
                     _In_ PUCHAR Buffer,
                     _In_ ULONGLONG Offset,
                     _Inout_ PULONGLONG Length)
{
    ULONGLONG BytesToWrite, BytesRead, BytesInRun, DataPointer;
    PDataRun Head, CurrentDR;

#ifdef NTFS_DEBUG
    ASSERT(Buffer);
    ASSERT(Attr);
    ASSERT(Length);
    ASSERT(!Offset);
#endif

    DPRINT1("Copy data called!\n");

    if (!(Attr->IsNonResident))
    {
        DPRINT1("Attribute is resident!\n");

        // Determine number of bytes we need to write.
        BytesToWrite = (Attr->Resident.DataLength);

        DPRINT1("Bytes to write: %ld\n", BytesToWrite);

        // If the buffer is too small, fail.
        if (*Length < BytesToWrite)
            return STATUS_BUFFER_TOO_SMALL;

        // Copy attribute data into buffer.
        RtlCopyMemory(Buffer, GetResidentDataPointer(Attr), BytesToWrite);

        // Adjust length for caller.
        *Length -= BytesToWrite;

        DPRINT1("Length is now: %ld\n", *Length);

        return STATUS_SUCCESS;
    }

    else
    {
        DPRINT1("Attribute is non-resident!\n");

        // Attribute is nonresident.
        Head = FindNonResidentData(Attr);
        CurrentDR = Head;
        BytesRead = 0;
        DataPointer = 0;

        /// We only support non-fragmented files for now.
        ASSERT(!(CurrentDR->NextRun));

        while(CurrentDR)
        {
            // Get data run length
            BytesInRun = (CurrentDR->Length) * Volume->BytesPerSector * Volume->SectorsPerCluster;
            BytesRead = min(*Length, BytesInRun);

            // Get data (TODO: Replace with DumpBlocks)
            ReadBlock(Volume->PartDeviceObj,
                      (CurrentDR->LCN) * (Volume->SectorsPerCluster),
                      BytesRead / Volume->BytesPerSector,
                      Volume->BytesPerSector,
                      Buffer,
                      TRUE);

            // Set up next data run.
            CurrentDR = CurrentDR->NextRun;
        }

        *Length -= BytesRead;

done:
        // Free data run
        FreeDataRun(Head);

        return STATUS_SUCCESS;
    }
}