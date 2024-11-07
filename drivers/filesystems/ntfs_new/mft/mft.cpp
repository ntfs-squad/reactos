/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

/* INCLUDES *****************************************************************/
/* Get File Record Size (Bytes).
 * If clusters per file record is less than 0, the file record size is 2^(-ClustersPerFileRecord).
 * Otherwise, the file record size is ClustersPerFileRecord * SectorsPerCluster * BytesPerSector.
*/
#define GetFileRecordSize(ClustersPerFileRecord, BytesPerCluster) \
ClustersPerFileRecord < 0 ? \
1 << (-(ClustersPerFileRecord)) : \
ClustersPerFileRecord * BytesPerCluster

#define IsRootFile(Path) \
Path[0] == L'\0' || (Path[0] == L'\\' && Path[1] == L'\0')

#include "../io/ntfsprocs.h"

MFT::MFT(_In_ PNTFSVolume TargetVolume,
         _In_ UINT64 MFTLCN,
         _In_ UINT64 MFTMirrLCN,
         _In_ INT8   ClustersPerFileRecord)
{

    /* TODO: We need to implement VCN-to-LCN mapping.
     * From Windows Internals 7th ed, Part 2:
     * "Once NTFS finds the file record for the MFT, it obtains the VCN-to-LCN mapping information
     * in the file record’s data attribute and stores it into memory. Each run (runs are explained
     * later in this chapter in the section “Resident and nonresident attributes”) has a VCN-to-LCN
     * mapping and a run length because that’s all the information necessary to locate the LCN for
     * any VCN. This mapping information tells NTFS where the runs containing the MFT are located
     * on the disk. NTFS then processes the MFT records for several more metadata files and opens
     * the files. Next, NTFS performs its file system recovery operation (described in the section
     * “Recovery” later in this chapter), and finally, it opens its remaining metadata files. The
     * volume is now ready for user access."
     */

    Volume = TargetVolume;
    this->MFTLCN = MFTLCN;
    this->MFTMirrLCN = MFTMirrLCN;

    FileRecordSize = GetFileRecordSize(ClustersPerFileRecord, BytesPerCluster(Volume));
}

NTSTATUS
MFT::GetFileRecord(_In_   ULONGLONG FileRecordNumber,
                   _Out_  PFileRecord* File)
{
    PAGED_CODE();

    ULONGLONG FileRecordOffset;

    // HACK! Use VCN-to-LCN mapping.
    FileRecordOffset = (MFTLCN * BytesPerCluster(Volume)) + (FileRecordNumber * FileRecordSize);

    *File = new(PagedPool) FileRecord(Volume,
                                      FileRecordOffset,
                                      FileRecordSize);

    return STATUS_SUCCESS;
}

// TODO: Handle wildcards and comparators
NTSTATUS
MFT::GetFileRecordFromQuery(_In_ PWCHAR Query,
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

    CurrentDirectory = new(PagedPool) Directory(Volume);

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
            CurrentDirectory = new(PagedPool) Directory(Volume);
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