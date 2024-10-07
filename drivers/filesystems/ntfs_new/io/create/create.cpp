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

    ULONG FileRecordNumber;

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

    // TODO: Check if we have rights to access file here.

    // Create file context block.
    FileCB = new(PagedPool) FileContextBlock();
    RtlZeroMemory(FileCB, sizeof(FileContextBlock));

    // Set file name.
    RtlCopyMemory(FileCB->FileName,
                  IrpSp->FileObject->FileName.Buffer,
                  IrpSp->FileObject->FileName.Length);

    // Hack: pick a file record number
    if (wcscmp(L"\\", FileName) == 0)
        FileRecordNumber = _Root;
    else if (wcscmp(L"\\folder\\", FileName) == 0)
        FileRecordNumber = 38;
    else
        FileRecordNumber = 30;

    CurrentFile = new(PagedPool) FileRecord(VolCB->Volume);

    VolCB->Volume->GetFileRecord(FileRecordNumber, CurrentFile);
    Attr = CurrentFile->GetAttribute(TypeStandardInformation,
                                        NULL);
    StdInfo = new(PagedPool) StandardInformationEx();
    RtlCopyMemory(StdInfo,
                    GetResidentDataPointer(Attr),
                    sizeof(StandardInformationEx));
    FileCB->FileRecordNumber = FileRecordNumber;

    // From file record
    FileCB->NumberOfLinks = CurrentFile->Header->HardLinkCount;
    FileCB->IsDirectory = !!(CurrentFile->Header->Flags & FR_IS_DIRECTORY);

    // From standard information
    FileCB->CreationTime.QuadPart = StdInfo->CreationTime;
    FileCB->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
    FileCB->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
    FileCB->ChangeTime.QuadPart = StdInfo->ChangeTime;
    FileCB->FileAttributes = StdInfo->FilePermissions;

    // Add pointer for file record
    FileCB->FileRec = CurrentFile;

    // Set FsContext to the file context block and open file.
    FileObject->FsContext = FileCB;
    Irp->IoStatus.Information = FILE_OPENED;

    DPRINT1("Opened file!\n");

    return STATUS_SUCCESS;
}
