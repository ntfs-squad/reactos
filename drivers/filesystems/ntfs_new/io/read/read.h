#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

static
PUCHAR
GetUserBuffer(PIRP Irp,
              BOOLEAN Paging)
{
    if (Irp->MdlAddress != NULL)
        return (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, (Paging ? HighPagePriority : NormalPagePriority));

    else
        return (PUCHAR)Irp->UserBuffer;
}

static
NTSTATUS
ReadFile(_In_  PFileContextBlock FileCB,
         _In_  ULONG Offset,
         _In_  ULONG RequestedLength,
         _Out_ PUCHAR Buffer,
         _Out_ PULONG ReadLength)
{
    NTSTATUS Status;
    PFileRecord FileRecord;
    PAttribute CurrentAttribute;
    PStandardInformationEx StdInfoAttrEx;

    // If we aren't reading anything, don't read anything.
    if (!RequestedLength)
        return STATUS_SUCCESS;

    FileRecord = FileCB->FileRec;

    if (!FileCB || !FileCB->FileRecordNumber || !FileRecord)
    {
        // If there is no file record, we can't find the file.
        DPRINT1("File context block is invalid!\n");
        return STATUS_FILE_NOT_AVAILABLE;
    }

    // Get standard information for file.
    CurrentAttribute = FileRecord->GetAttribute(TypeStandardInformation, NULL);
    StdInfoAttrEx = (StandardInformationEx*)(GetResidentDataPointer(CurrentAttribute));

    // Check if file is compressed.
    if (StdInfoAttrEx->FilePermissions & FILE_PERM_COMPRESSED)
    {
        DPRINT1("File Record is compressed!\n");
        Status = STATUS_NOT_IMPLEMENTED;
        goto cleanup;
    }

    // Check if file is encrypted.
    if(StdInfoAttrEx->FilePermissions & FILE_PERM_ENCRYPTED)
    {
        DPRINT1("File Record is encrypted!\n");
        Status = STATUS_NOT_IMPLEMENTED;
        goto cleanup;
    }

    // TODO: COMPLETE!!!
    *ReadLength = RequestedLength;
    CurrentAttribute = FileRecord->GetAttribute(TypeData, NULL);
    Status = FileRecord->CopyData(CurrentAttribute,
                                  Buffer,
                                  Offset,
                                  &RequestedLength);
    *ReadLength -= RequestedLength;

cleanup:
    return Status;
}