#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

#define GetUserBuffer(Irp) Irp->MdlAddress ?\
MmGetSystemAddressForMdlSafe(Irp->MdlAddress, ((Irp->Flags & IRP_PAGING_IO) ? HighPagePriority : NormalPagePriority)) :\
Irp->UserBuffer
#define GetBuffer(Irp) Irp->AssociatedIrp.SystemBuffer ? Irp->AssociatedIrp.SystemBuffer : GetUserBuffer(Irp)

static
NTSTATUS
ReadFile(_In_  PFileContextBlock FileCB,
         _In_  ULONG Offset,
         _In_  PULONG RequestedLength,
         _Out_ PUCHAR Buffer)
{
    NTSTATUS Status;

    // If we aren't reading anything, don't read anything.
    if (!RequestedLength)
        return STATUS_SUCCESS;

#ifdef NTFS_DEBUG
    ASSERT(FileCB);
    ASSERT(FileCB->FileRecordNumber);
    ASSERT(FileCB->FileRec);
    ASSERT(!(FileCB->FileAttributes & FILE_PERM_COMPRESSED));
    ASSERT(!(FileCB->FileAttributes & FILE_PERM_ENCRYPTED));
#endif

    // Copy data from $DATA into file buffer.
    Status = FileCB->FileRec->CopyData(TypeData, NULL, Buffer, RequestedLength, Offset);

cleanup:
    return Status;
}