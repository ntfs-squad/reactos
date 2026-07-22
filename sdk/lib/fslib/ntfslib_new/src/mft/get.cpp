/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

// NOTE: This function will search $MFTMirr automatically if needed.
NTSTATUS
MasterFileTable::GetFileRecord(_In_   ULONG FileRecordNumber,
                               _Out_  PFileRecord* File)
{
    NTSTATUS Status;
    ULONG BytesToRead;
    BOOLEAN IsRecordInUse;

    *File = new(PagedPool, TAG_MFT) FileRecord(DiskVolume, FileRecordSize);
    BytesToRead = FileRecordSize;

    if (FileRecordNumber == _MFT)
    {
        // The $MFT file is calculated from the MFT LCN.
        Status = DiskVolume->ReadVolume(MFTDiskOffset,
                                        FileRecordSize,
                                        (*File)->Data);
        goto FileCheck;
    }

    else if (FileRecordNumber == _MFTMirr)
    {
        // The $MFTMirr file is calculated from the MFTMirr LCN.
        Status = DiskVolume->ReadVolume(MFTMirrDiskOffset,
                                        FileRecordSize,
                                        (*File)->Data);
        goto FileCheck;
    }

    /* All other files are found by querying the MFT file or if that fails
     * and the file is in $MFTMirr, the MFTMirr file.
     */

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

    // Grab file from $MFT, reusing the cached $DATA attribute and run list.
    Status = MFTDataAttr ?
             MFTFile->CopyData(MFTDataAttr,
                               MFTDataRuns,
                               (*File)->Data,
                               &BytesToRead,
                               FileRecordOffset(FileRecordNumber))
             : STATUS_NOT_FOUND;

FileCheck:
    /* TODO: More extensive checking needed to pass file check here.
     * Probably need to make sure attributes are aligned to 8-byte boundaries
     * and that the length of attributes make sense.
     */
    if (!NT_SUCCESS(Status) ||
        !(RtlCompareMemory((*File)->Header->Header.TypeID, "FILE", 4) == 4))
    {
        DPRINT1("Failed to get file %ld from MFT!\n", FileRecordNumber);
        delete *File;
        if (FileRecordNumber != _MFT && FileRecordNumber != _MFTMirr)
            return GetFileRecordFromMFTMirr(FileRecordNumber, File);
        else
            return STATUS_NOT_FOUND;
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
    delete *File;
    return NT_SUCCESS(Status) ? STATUS_FILE_CORRUPT_ERROR : Status;
}

NTSTATUS
MasterFileTable::GetFileRecordFromMFTMirr(_In_   ULONG FileRecordNumber,
                                          _Out_  PFileRecord* File)
{
    NTSTATUS Status;
    BOOLEAN IsRecordInUse;
    ULONG BytesToRead;

    if (!IsFileRecordInMFTMirr(FileRecordNumber))
        return STATUS_NOT_FOUND;

    if (FileRecordNumber == _MFT ||
        FileRecordNumber == _MFTMirr)
    {
        // Logic for finding MFT and MFTMirr is in GetFileRecord()
        return GetFileRecord(FileRecordNumber, File);
    }

    // If the file record is not in use, fail.
    Status = IsFileRecordNumberInUse(FileRecordNumber,
                                     &IsRecordInUse);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to determine if file record number is in use!\n");
        return Status;
    }

    if (!IsRecordInUse)
    {
        DPRINT1("File record is not in use!\n");
        return Status;
    }

    // File is in MFTMirr. Let's try to get it from there.
    *File = new(PagedPool, TAG_MFT) FileRecord(DiskVolume, FileRecordSize);
    BytesToRead = FileRecordSize;

    // Grab file from $MFTMirr, reusing the cached $DATA attribute and run list.
    Status = MFTMirrDataAttr ?
             MFTMirrFile->CopyData(MFTMirrDataAttr,
                                   MFTMirrDataRuns,
                                   (*File)->Data,
                                   &BytesToRead,
                                   FileRecordOffset(FileRecordNumber))
             : STATUS_NOT_FOUND;

    if (!NT_SUCCESS(Status) ||
        !(RtlCompareMemory((*File)->Header->Header.TypeID, "FILE", 4) == 4))
    {
        DPRINT1("Failed to get file %ld from MFT Mirror!\n", FileRecordNumber);
        goto Failed;
    }

    // Apply fixup for the file.
    Status = (*File)->ApplyFixup();

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to apply fixup!\n");
        goto Failed;
    }

    return Status;

Failed:
    delete *File;
    return NT_SUCCESS(Status) ? STATUS_FILE_CORRUPT_ERROR : Status;
}

// Utilizes MFTMirr to find an attribute if the MFT version doesn't have it.
NTSTATUS
MasterFileTable::GetFileAttributeFromFileRecordNumber(_In_  AttributeType Type,
                                                      _In_  PWSTR Name,
                                                      _In_  ULONG FileRecordNumber,
                                                      _Out_ PFileRecord* TargetFile,
                                                      _Out_ PAttribute* TargetAttribute)
{
    NTSTATUS Status;

    // Grab the file using GetFileRecord().
    Status = GetFileRecord(FileRecordNumber, TargetFile);
    if (!NT_SUCCESS(Status))
        return Status;

    *TargetAttribute = (*TargetFile)->GetAttribute(Type, Name);

    if (!TargetAttribute)
    {
        // Free current target file.
        delete *TargetFile;

        // Try getting the attribute using the file from MFTMirr.
        Status = GetFileRecordFromMFTMirr(FileRecordNumber, TargetFile);
        if (!NT_SUCCESS(Status))
            return Status;

        *TargetAttribute = (*TargetFile)->GetAttribute(Type, Name);
        if (!TargetAttribute)
        {
            // We still don't have the attribute.
            delete *TargetFile;
            return STATUS_NOT_FOUND;
        }
    }

    return Status;
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

    if (!Query)
    {
        DPRINT1("INVESTIGATE ME: GetFileRecordFromQuery() called with NULL Query!\n");
        return STATUS_INVALID_PARAMETER;
    }

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

    CurrentDirectory = new(PagedPool, TAG_MFT) Directory(DiskVolume);

    Status = CurrentDirectory->LoadDirectory(CurrentFile);
    if(!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get root directory!\n");
        return STATUS_NOT_FOUND;
    }

    QueryElementPtr = Query;
    QueryElementPtr = wcschr(QueryElementPtr, L'\\');
    if (QueryElementPtr)
        QueryElementPtr++;
    else
        QueryElementPtr = Query;

    while(QueryElementPtr)
    {
        // Find the directory pointed to by the path
        Status = CurrentDirectory->FindNextFile(QueryElementPtr,
                                                &CurrentFRN);
        if (!NT_SUCCESS(Status))
        {
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
            CurrentDirectory = new(PagedPool, TAG_MFT) Directory(DiskVolume);
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
            QueryElementPtr++;
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