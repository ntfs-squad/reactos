#include "io/ntfsprocs.h"

/* *** FILE RECORD IMPLEMENTATIONS *** */
FileRecord::FileRecord(PNTFSVolume ThisVolume)
{
    Volume = ThisVolume;
}

NTSTATUS
FileRecord::LoadData(_In_ PUCHAR FileRecordData,
                     _In_ UINT Length)
{
    PAGED_CODE();
    Data = new(PagedPool) UCHAR[Length];
    RtlCopyMemory(Data, FileRecordData, Length);
    Header = (PFileRecordHeader)Data;
    return STATUS_SUCCESS;
}
