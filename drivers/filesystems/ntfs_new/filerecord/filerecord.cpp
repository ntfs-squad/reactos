#include "io/ntfsprocs.h"

/* *** FILE RECORD IMPLEMENTATIONS *** */
FileRecord::FileRecord(_In_ PNTFSVolume ThisVolume,
                       _In_ ULONGLONG FileRecordNumber)
{

    /* TODO: We need to use VCN-to-LCN mapping.
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

    PAGED_CODE();

    Volume = ThisVolume;

    Data = new(PagedPool) UCHAR[Volume->FileRecordSize];

    ReadDisk(Volume->PartDeviceObj,
             (Volume->MFTLCN * Volume->SectorsPerCluster * Volume->BytesPerSector) + (FileRecordNumber * Volume->FileRecordSize),
             Volume->FileRecordSize,
             Data);

    Header = (PFileRecordHeader)Data;
}
