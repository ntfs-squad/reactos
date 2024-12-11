/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file creation APIs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

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
    PFileContextBlock FileCB;
    NTSTATUS Status;
    PFILE_OBJECT FileObject;
    BOOLEAN PerformAccessChecks;
    FileRecord* CurrentFile;
    UINT8 Disposition;
    PNTFSVolume Volume;

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
    Disposition = GetDisposition(IrpSp->Parameters.Create.Options);
    Volume = ((PVolumeContextBlock)VolumeDeviceObject->DeviceExtension)->Volume;

    // Determine if we should check access rights
    PerformAccessChecks = (Irp->RequestorMode == UserMode) ||
                          (IrpSp->Flags & SL_FORCE_ACCESS_CHECK);

    // TODO: Check if we have rights to access file.

    // Try to find the requested file record.
    Status = Volume->MFT->GetFileRecordFromQuery(FileObject->FileName.Buffer,
                                                 &CurrentFile);

    /* What we do here depends on the CreateDisposition value.
     * See https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntcreatefile
     */

    if (NT_SUCCESS(Status))
    {
        // The file was found.

        // In this case, return an error.
        if (Disposition == FILE_CREATE)
        {
            Irp->IoStatus.Information = FILE_EXISTS;
            return STATUS_INVALID_PARAMETER;
        }

        // In every other case, we should continue to open the file.
    }

    else
    {
        // The file was not found.

        switch (Disposition)
        {
            case FILE_SUPERSEDE:
            case FILE_CREATE:
            case FILE_OPEN_IF:
            case FILE_OVERWRITE_IF:
                /* In these cases, create the file and open it.
                 * Algorithm will probably be something like:
                 *     - Call MFT to allocate a new file record.
                 *     - Add $FILE_NAME attribute to parent directory tree.
                 *     - Set new file record to CurrentFile to open it.
                 * MFT will handle finding a free RecordID and calling LFS.
                 */
                DPRINT1("File creation not implemented! File: \"%S\"\n",
                        FileObject->FileName.Buffer);
                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                return STATUS_NOT_IMPLEMENTED;
                break;
            case FILE_OPEN:
            case FILE_OVERWRITE:
            default:
                // In these cases, return an error.
                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                return STATUS_INVALID_PARAMETER;
                break;
        }

    }

    // Create file context block.
    FileCB = new(NonPagedPool) FileContextBlock();
    RtlZeroMemory(FileCB, sizeof(FileContextBlock));

    // Set file name
    // TODO: Axe?
    RtlCopyMemory(FileCB->FileName,
                  IrpSp->FileObject->FileName.Buffer,
                  IrpSp->FileObject->FileName.Length);

    // Get ADS Preferences for the file.
    Status = Volume->GetADSPreference(FileObject,
                                      &FileCB->RequestedType,
                                      &FileCB->RequestedStream);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get ADS preference! Aborting...\n");
        delete FileCB;
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        return Status;
    }

    FileCB->FileRec = CurrentFile;
    FileCB->CreateOptions = IrpSp->Parameters.Create.Options;
    FileCB->DesiredAccess = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;

    /* Assume that this is the first file stream request.
     * For more details see:
     * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_section_object_pointers
     *
     * TODO: Handle multiple opened files pointing to the same stream properly.
     */
    FileCB->StreamCB = new(NonPagedPool) StreamContextBlock();
    FileCB->StreamCB->SectionObjectPointers = {0};

    if (!!(CurrentFile->Header->Flags & FR_IS_DIRECTORY))
    {
        // Set up btree for this file
        FileCB->FileDir = new(PagedPool) Directory(Volume);
        Status = FileCB->FileDir->LoadDirectory(FileCB->FileRec);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to get directory!\n");
            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
            return Status;
        }
    }

    // Set FsContext to the file context block and open file.
    FileObject->FsContext = FileCB;
    Irp->IoStatus.Information = FILE_OPENED;

    return STATUS_SUCCESS;
}
