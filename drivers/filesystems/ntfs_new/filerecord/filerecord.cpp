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
    RtlCopyMemory(Data, FileRecordData, Length);
    return STATUS_SUCCESS;
}
