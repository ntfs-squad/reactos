/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

/* INCLUDES *****************************************************************/
#include "ntfspch.h"

#define IsRootFile(Path) \
Path[0] == L'\0' || (Path[0] == L'\\' && Path[1] == L'\0')

#define InvalidMftZoneReservation(Num) Num < 1 || Num > 4

#define IsFileRecordInMFTMirr(FileRecordNumber) \
((Volume->SectorsPerCluster * Volume->BytesPerSector) > (FileRecordSize << 2)) ? \
((Volume->SectorsPerCluster * Volume->BytesPerSector) / FileRecordSize) < FileRecordNumber \
: FileRecordNumber < 4

#define MFTDiskOffset (MFTLCN * BytesPerCluster(Volume))
#define MFTMirrDiskOffset (MFTMirrLCN * BytesPerCluster(Volume))
#define FileRecordOffset(FileRecordNumber) (FileRecordNumber * FileRecordSize)


MasterFileTable::MasterFileTable(_In_ PNTFSVolume TargetVolume,
                                 _In_ UINT64 MFTLCN,
                                 _In_ UINT64 MFTMirrLCN,
                                 _In_ INT8   ClustersPerFileRecord)
{
    Volume = TargetVolume;
    this->MFTLCN = MFTLCN;
    this->MFTMirrLCN = MFTMirrLCN;

    /* Set the file record size, in bytes.
     * If clusters per file record is less than 0, the file record size is 2^(-ClustersPerFileRecord).
     * Otherwise, the file record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector.
     */
    FileRecordSize = ClustersPerFileRecord < 0 ?
                     1 << (-(ClustersPerFileRecord))
                     : ClustersPerFileRecord * BytesPerCluster(Volume);

    /* Get the MftReservationZone registry value.
     * Per Microsoft Learn: This value should be between 1 and 4. 1 is the default.
     * TODO: Actually use it for MFT Zone reservations.
     */
    MftZoneReservation = QueryDwordRegistryValue(L"NtfsMftZoneReservation", 1);
    if (InvalidMftZoneReservation(MftZoneReservation))
        MftZoneReservation = 1;
}

// TODO: Utilize MFT $BITMAP to determine if file record is in use.
NTSTATUS
MasterFileTable::GetFileRecord(_In_   ULONG FileRecordNumber,
                               _Out_  PFileRecord* File)
{
    PAGED_CODE();
    NTSTATUS Status;
    ULONG BytesToRead;
    BOOLEAN IsRecordInUse;

    *File = new(PagedPool, TAG_MFT) FileRecord(Volume);
    BytesToRead = FileRecordSize;

    if (FileRecordNumber == _MFT)
    {
        // The $MFT file is calculated from the MFT LCN.
        Status = Volume->ReadVolume(MFTDiskOffset,
                                    FileRecordSize,
                                    (*File)->Data);
    }

    else if (FileRecordNumber == _MFTMirr)
    {
        // The $MFTMirr file is calculated from the MFTMirr LCN.
        Status = Volume->ReadVolume(MFTMirrDiskOffset,
                                    FileRecordSize,
                                    (*File)->Data);
    }

    else
    {
        /* All other files are found by querying the MFT file or if that fails
         * and the file is in $MFTMirr, the MFTMirr file.
         */

        // Initialize MFT file if it isn't already.
        if (!MFTFile)
        {
            Status = GetFileRecord(_MFT, &MFTFile);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to get $MFT File!\n");
                goto MFTFailed;
            }
        }

        // If the file record is not in use, fail.
        Status = IsFileRecordNumberInUse(FileRecordNumber,
                                         &IsRecordInUse);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to determine if file record number is in use!\n");
            goto Failed;
        }

        if (!IsRecordInUse)
        {
            DPRINT1("File record is not in use!\n");
            goto Failed;
        }

        Status = MFTFile->CopyData(TypeData,
                                   NULL,
                                   (*File)->Data,
                                   &BytesToRead,
                                   FileRecordOffset(FileRecordNumber));
    }

MFTFailed:
    if (!NT_SUCCESS(Status) ||
        !(RtlCompareMemory((*File)->Header->Header.TypeID, "FILE", 4) == 4))
    {
        DPRINT1("Failed to get file %ld from MFT!\n", FileRecordNumber);

        // Check if we can get the file from MFTMirr
        if (IsFileRecordInMFTMirr(FileRecordNumber))
        {
            // Initialize MFTMirr file if it isn't already.
            if (!MFTMirrFile)
            {
                Status = GetFileRecord(_MFTMirr, &MFTMirrFile);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("Failed to get file from MFT Mirror!\n");
                    goto Failed;
                }
            }

            // File is in MFTMirr. Let's try to get it from there.
            BytesToRead = FileRecordSize;
            Status = MFTMirrFile->CopyData(TypeData,
                                           NULL,
                                           (*File)->Data,
                                           &BytesToRead,
                                           FileRecordOffset(FileRecordNumber));
            if (!NT_SUCCESS(Status) ||
                !(RtlCompareMemory((*File)->Header->Header.TypeID, "FILE", 4) == 4))
            {
                DPRINT1("Failed to get file from MFT Mirror!\n");
                goto Failed;
            }

            /* If we're here, that means we were able to get the file from
             * $MFTMirr. Proceed as normal.
             */
            DPRINT1("Got file from $MFTMirr!\n");
        }

        else
        {
            // File is not in MFTMirr.
            DPRINT1("File is not in the MFT Mirror!\n");
            goto Failed;
        }
    }

    // Apply fixup for the file.
    Status = (*File)->ApplyFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("File corruption detected!\n");
        goto Failed;
    }

    return Status;

Failed:
    __debugbreak();
    delete *File;
    return NT_SUCCESS(Status) ? STATUS_FILE_CORRUPT_ERROR : Status;
}

NTSTATUS
MasterFileTable::GetFileRecordFromQuery(_In_ PWCHAR Query,
                                        _Out_ PFileRecord* File)
{
    NTSTATUS Status;
    PWCHAR QueryElementPtr;
    Directory* CurrentDirectory;
    PFileRecord CurrentFile;
    ULONGLONG CurrentFRN;

    DPRINT1("Looking for file: \"%S\"\n", Query);

    // Let's start with the root directory, which is hardcoded.
    if (IsRootFile(Query))
        return GetFileRecord(_Root, File);

    /* Every other file will be inside of the root directory.
     * Parse the query to find the file record.
     */
    Status = GetFileRecord(_Root, &CurrentFile);
    if(!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get root directory file!\n");
        return STATUS_NOT_FOUND;
    }

    CurrentDirectory = new(PagedPool, TAG_MFT) Directory(Volume);

    Status = CurrentDirectory->LoadDirectory(CurrentFile);
    if(!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get root directory!\n");
        return STATUS_NOT_FOUND;
    }

    QueryElementPtr = Query;
    QueryElementPtr = wcschr(QueryElementPtr, L'\\');
    QueryElementPtr = &QueryElementPtr[1];

    while(QueryElementPtr)
    {
        DPRINT1("Searching for: \"%S\"\n", QueryElementPtr);
        // Find the directory pointed to by the path
        Status = CurrentDirectory->FindNextFile(QueryElementPtr,
                                                &CurrentFRN);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to find \"%S\"!\n", QueryElementPtr);
            delete CurrentDirectory;
            delete CurrentFile;
            return STATUS_NOT_FOUND;
        }

        delete CurrentDirectory;
        delete CurrentFile;

        Status = GetFileRecord(CurrentFRN, &CurrentFile);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to find file at MFT ID %ld!\n", CurrentFRN);
            delete CurrentDirectory;
            return STATUS_NOT_FOUND;
        }

        // Second condition is to check "//folder//target//"
        // Is this a hack or fix?
        if (wcschr(QueryElementPtr, L'\\') &&
            wcschr(QueryElementPtr, L'\\')[1] != L'\0')
        {
            CurrentDirectory = new(PagedPool, TAG_MFT) Directory(Volume);
            Status = CurrentDirectory->LoadDirectory(CurrentFile);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to find directory for file at MFT ID %ld!\n", CurrentFRN);
                delete CurrentDirectory;
                delete CurrentFile;
                return STATUS_NOT_FOUND;
            }

            // Set up next file
            QueryElementPtr = wcschr(QueryElementPtr, L'\\');
            QueryElementPtr = &QueryElementPtr[1];
        }

        else
        {
            break;
        }
    }

    // Current File should now point to the queried file.
    *File = CurrentFile;
    return STATUS_SUCCESS;
}

NTSTATUS
MasterFileTable::WriteFileRecordToMFT(_In_ PFileRecord File)
{
    NTSTATUS Status;
    ULONGLONG FileRecordDiskOffset;

    // TODO: Add logging here

    // Insert the fixup array into the file record in memory.
    Status = File->CommitFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to commit fixup! (Status: 0x%X)\n", Status);
        return Status;
    }

    // HACK! Use VCN-to-LCN mapping (needed for fragmented MFTs).
    FileRecordDiskOffset = (MFTLCN * BytesPerCluster(Volume))
                           + (File->Header->MFTRecordNumber * FileRecordSize);

    // Write to disk.
    Status = WriteDisk(Volume->PartDeviceObj,
                       FileRecordDiskOffset,
                       FileRecordSize,
                       File->Data);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to write to disk! (Status: 0x%X)\n", Status);
        return Status;
    }

    if (IsFileRecordInMFTMirr(File->Header->MFTRecordNumber))
    {
        /* TODO: Is it even possible that an MFT mirror can become fragmented?
         * Very rarely would it ever be larger than one cluster.
         */
        FileRecordDiskOffset = (MFTMirrLCN * BytesPerCluster(Volume))
                               + (File->Header->MFTRecordNumber * FileRecordSize);

        // Write to disk.
        Status = WriteDisk(Volume->PartDeviceObj,
                           FileRecordDiskOffset,
                           FileRecordSize,
                           File->Data);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Unable to write to MFT Mirror! (Status: 0x%X)\n", Status);
            return Status;
        }
    }

    // Undo the fixup array to fix the file record in memory.
    Status = File->ApplyFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Unable to revert fixup! (Status: 0x%X)\n", Status);
        return Status;
    }

    return Status;
}

NTSTATUS
MasterFileTable::IsFileRecordNumberInUse(_In_  ULONG FileRecordNumber,
                                         _Out_ PBOOLEAN InUse)
{
#if 0
    NTSTATUS Status;
    USHORT Bitmask;
    UCHAR BitmapSection;
    ULONG Size;

    /* This code consistently fails an assertion:
     * .\drivers\storage\class\disk\disk.c(589): residualOffset == 0
     */

    Size = 1;
    Status = MFTFile->CopyData(TypeBitmap,
                               NULL,
                               &BitmapSection,
                               &Size,
                               FileRecordNumber >> 3);

    Bitmask = 1 << (FileRecordNumber % 8);

    if(!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get bitmap!\n");
        return Status;
    }

    *InUse = !!(BitmapSection & Bitmask);
    return STATUS_SUCCESS;
#else
    *InUse = TRUE;
    return STATUS_SUCCESS;
#endif
}