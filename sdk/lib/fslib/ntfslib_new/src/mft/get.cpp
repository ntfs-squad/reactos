/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

// NOTE: This function will search $MFTMirr automatically if needed.
NTSTATUS
MasterFileTable::GetFileRecord(_In_   ULONGLONG FileRecordNumber,
                               _Out_  PFileRecord* File)
{
    NTSTATUS Status;
    ULONG BytesToRead;
    BOOLEAN IsRecordInUse;

    NtfsAssertPagedCode();
    *File = new(NtfsPagedPool, TAG_MFT) FileRecord(DiskVolume, FileRecordSize);
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

    // Grab file from $MFT
    Status = MFTFile ?
             MFTFile->CopyData(TypeData,
                               NULL,
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
        DPRINT1("Failed to get file %I64u from MFT!\n", FileRecordNumber);
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
MasterFileTable::GetFileRecordFromMFTMirr(_In_   ULONGLONG FileRecordNumber,
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
    *File = new(NtfsPagedPool, TAG_MFT) FileRecord(DiskVolume, FileRecordSize);
    BytesToRead = FileRecordSize;

    // Grab file from $MFTMirr
    Status = MFTMirrFile ?
             MFTMirrFile->CopyData(TypeData,
                                   NULL,
                                   (*File)->Data,
                                   &BytesToRead,
                                   FileRecordOffset(FileRecordNumber))
             : STATUS_NOT_FOUND;

    if (!NT_SUCCESS(Status) ||
        !(RtlCompareMemory((*File)->Header->Header.TypeID, "FILE", 4) == 4))
    {
        DPRINT1("Failed to get file %I64u from MFT Mirror!\n", FileRecordNumber);
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
                                                      _In_  ULONGLONG FileRecordNumber,
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
    PFileRecord Record;

    if (!Query)
        return STATUS_INVALID_PARAMETER;

    if (*Query == L'\\')
        ++Query;

    Status = GetFileRecord(_Root, &Record);
    if (!NT_SUCCESS(Status))
        return Status;

    while (*Query)
    {
        // Find the next separator to get to the end of the current file path token.
        PWCHAR End = Query;
        while (*End && *End != L'\\' && *End != L':')
            ++End;

        // Determine string length for current file path token.
        USHORT Length = (End - Query) * sizeof(WCHAR);
        if (!Length)
        {
            delete Record;
            return STATUS_NOT_FOUND;
        }

        UNICODE_STRING Component;
        RtlInitEmptyUnicodeString(&Component, Query, Length);
        Component.Length = Component.MaximumLength;

        // Find the file record that represents the current file path token.
        ULONGLONG FileRecordNumber;
        {
            // Optimization: We don't need to worry about short name keys for this lookup.
            Directory Index(DiskVolume);
            Status = Index.LoadDirectory(Record, TRUE);
            delete Record;
            if (!NT_SUCCESS(Status))
                return STATUS_NOT_FOUND;

            Status = Index.FindFile(&Component, &FileRecordNumber);
        }
        if (!NT_SUCCESS(Status))
            return Status;

        // Get the file record from the file record number
        Status = GetFileRecord(FileRecordNumber, &Record);
        if (!NT_SUCCESS(Status))
            return Status;

        /* If the next separator is not a backslash, break.
         * Either, the path is already at its end;
         * Or it's on the ADS operator ':', which all use the same file record.
         */
        if (*End != L'\\')
            break;

        // Advance Query to after the last separator
        Query = End + 1;
    }

    *File = Record;
    return STATUS_SUCCESS;
}
