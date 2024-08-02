#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

static
PVOID
GetUserBuffer(PIRP Irp,
              BOOLEAN Paging)
{
    if (Irp->MdlAddress != NULL)
        return MmGetSystemAddressForMdlSafe(Irp->MdlAddress, (Paging ? HighPagePriority : NormalPagePriority));

    else
        return Irp->UserBuffer;
}

static
NTSTATUS
ReadFile(_In_ PIO_STACK_LOCATION IoStack,
         _Out_ PVOID Buffer,
         _In_ ULONG RequestedLength,
         _Out_ PULONG ReadLength)
{
    NTSTATUS Status;
    PFileContextBlock FileCB;
    ResidentAttribute* StdInfoAttr;
    StandardInformationEx* StdInfoAttrEx;

    // If we aren't reading anything, don't read anything.
    if (!RequestedLength)
        return STATUS_SUCCESS;

    FileCB = (PFileContextBlock)IoStack->FileObject->FsContext;

    DPRINT1("File Context Block found!\n");

    if (!FileCB || !FileCB->FileRec)
    {
        // If there is no file record, we can't find the file.
        DPRINT1("File context block is invalid!\n");
        return STATUS_FILE_NOT_AVAILABLE;
    }

    DPRINT1("File record found!\n");

    // Get standard information for file.
    StdInfoAttr = (ResidentAttribute*)(FileCB->FileRec->FindAttributePointer(StandardInformation, NULL));
    StdInfoAttrEx = (StandardInformationEx*)(GetResidentDataPointer(StdInfoAttr));

    // Check if file is compressed.
    if (StdInfoAttrEx->FilePermissions & FILE_PERM_COMPRESSED)
    {
        UNIMPLEMENTED;
        Status = STATUS_NOT_IMPLEMENTED;
        goto cleanup;
    }

    // Check if file is encrypted.
    if(StdInfoAttrEx->FilePermissions & FILE_PERM_ENCRYPTED)
    {
        UNIMPLEMENTED;
        Status = STATUS_NOT_IMPLEMENTED;
        goto cleanup;
    }

    // TODO: COMPLETE!!!
    // Status = FileCB->FileRec->GetAttribute(Data, NULL, (PUCHAR)Buffer);

cleanup:
    return Status;
}