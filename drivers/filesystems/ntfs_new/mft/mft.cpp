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

    *File = new(PagedPool) FileRecord(Volume);
    (*File)->Data = new(PagedPool) UCHAR[FileRecordSize];

    ReadDisk(Volume->PartDeviceObj,
             (MFTLCN * BytesPerCluster(Volume))
             + (FileRecordNumber * FileRecordSize),
             FileRecordSize,
             (*File)->Data);

    (*File)->Header = (PFileRecordHeader)((*File)->Data);

    return STATUS_SUCCESS;
}