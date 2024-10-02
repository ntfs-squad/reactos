/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file creation APIs
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */


/* INCLUDES *****************************************************************/

#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdCreate)
#endif

/* FUNCTIONS ****************************************************************/
extern PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;

static
BOOLEAN
RejectFileOpen(PWCH FileName)
{
    // this whole function is a hack
    return !wcscmp(L"\\RECYCLED\\INFO2", FileName) ||
           !wcscmp(L"\\System Volume Information\\MountPointManagerRemoteDatabase", FileName) ||
           !wcscmp(L"\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION", FileName);
}

static
BOOLEAN
IsDirectory(PWCH FileName)
{
    // Hack implementation
    return wcscmp(L"\\*\\", FileName) == 0;
}

_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCreate(_In_ PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp)
{
    /* Overview:
     * Handle creation or opening of a file, device, directory, or volume.
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-create
     */

    PIO_STACK_LOCATION IrpSp;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    // NTSTATUS Status;
    PFILE_OBJECT FileObject;
    BOOLEAN PerformAccessChecks;
    PWCH FileName;
    FileRecord* CurrentFile;
    PAttribute Attr;
    StandardInformationEx* StdInfo;

    if (VolumeDeviceObject == NtfsDiskFileSystemDeviceObject)
    {
        /* DeviceObject represents FileSystem instead of logical volume */
        DPRINT1("Opening file system\n");
        Irp->IoStatus.Information = FILE_OPENED;
        return STATUS_SUCCESS;
    }

    // Investigate file request
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileObject = IrpSp->FileObject;
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    FileName = IrpSp->FileObject->FileName.Buffer;

    DPRINT1("On Partition \"%S\"\n", IrpSp->DeviceObject->Vpb->VolumeLabel);
    DPRINT1("Looking for file: \"%S\"\n", FileName);

    // Determine if we should check access rights
    PerformAccessChecks = (Irp->RequestorMode == UserMode) ||
                          (IrpSp->Flags & SL_FORCE_ACCESS_CHECK);

    // For now, don't open certain files. This is a hack!
    if (RejectFileOpen(FileName))
    {
        DPRINT1("Rejected opening file!\n");
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        return STATUS_NOT_FOUND;
    }

    // TODO: Check if we have rights to access file here.

    // Create file context block.
    FileCB = new(PagedPool) FileContextBlock();
    RtlZeroMemory(FileCB, sizeof(FileContextBlock));

    // Set file name.
    RtlCopyMemory(FileCB->FileName,
                  IrpSp->FileObject->FileName.Buffer,
                  IrpSp->FileObject->FileName.Length);

    // This only works on the root file for now.
    // ASSERT(wcscmp(L"\\", FileName) == 0);
    CurrentFile = new(PagedPool) FileRecord(VolCB->Volume);
    VolCB->Volume->GetFileRecord(30, CurrentFile);
    PrintFileRecordHeader(CurrentFile->Header);
    Attr = CurrentFile->GetAttribute(AttributeType::StandardInformation,
                                     NULL);
    PrintAttributeHeader(Attr);
    StdInfo = new(PagedPool) StandardInformationEx();
    RtlCopyMemory(StdInfo,
                  GetResidentDataPointer(Attr),
                  sizeof(StandardInformationEx));
    PrintStdInfoEx(StdInfo);
    FileCB->FileRecordNumber = 30;
    FileCB->NumberOfLinks = 1;

    // Fill out standard information
    FileCB->CreationTime.QuadPart = StdInfo->CreationTime;
    FileCB->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
    FileCB->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
    FileCB->ChangeTime.QuadPart = StdInfo->ChangeTime;
    FileCB->FileAttributes = StdInfo->FilePermissions;

    if (IsDirectory(FileName))
    {
        FileCB->IsDirectory = TRUE;
        FileCB->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    }

    // Set FsContext to the file context block and open file.
    FileObject->FsContext = FileCB;
    Irp->IoStatus.Information = FILE_OPENED;

    DPRINT1("Opened file!\n");

    return STATUS_SUCCESS;
}
