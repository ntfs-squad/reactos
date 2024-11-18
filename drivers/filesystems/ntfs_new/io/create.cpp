/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file creation APIs
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

/* INCLUDES *****************************************************************/

#include "ntfspch.h"

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdCreate)
#endif

#define GetDisposition(x) ((x >> 24) & 0xFF)
#define GetCreateOptions(x) (x & 0xFFFFFF)

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
    NTSTATUS Status;
    PFILE_OBJECT FileObject;
    BOOLEAN PerformAccessChecks;
    PWSTR FileNameQuery, ADSPtr, ADSTypePtr;
    FileRecord* CurrentFile;
    UINT8 Disposition;
    ULONG CreateOptions;
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
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    FileNameQuery = IrpSp->FileObject->FileName.Buffer;
    Disposition = GetDisposition(IrpSp->Parameters.Create.Options);
    CreateOptions = GetCreateOptions(IrpSp->Parameters.Create.Options);
    Volume = VolCB->Volume;

    // Determine if we should check access rights
    PerformAccessChecks = (Irp->RequestorMode == UserMode) ||
                          (IrpSp->Flags & SL_FORCE_ACCESS_CHECK);

    // Hack: Fail certain requests we aren't ready for
    if (Disposition == FILE_SUPERSEDE ||
        Disposition == FILE_CREATE ||
        Disposition == FILE_OVERWRITE ||
        Disposition == FILE_OVERWRITE_IF)
    {
        DPRINT1("Rejecting file open!\n");
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        return STATUS_NOT_IMPLEMENTED;
    }

    Status = Volume->MFT->GetFileRecordFromQuery(FileNameQuery, &CurrentFile);

    // TODO: Check if we have rights to access file here.

    if (!NT_SUCCESS(Status))
    {
        // This isn't always an issue, but it isn't implemented yet.
        DPRINT1("File not found! File: \"%S\"\n", FileNameQuery);
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        return Status;
    }

    // Create file context block.
    FileCB = new(PagedPool) FileContextBlock();
    RtlZeroMemory(FileCB, sizeof(FileContextBlock));

    // Set file name
    // TODO: Axe or get from $FILE_NAME
    RtlCopyMemory(FileCB->FileName,
                  IrpSp->FileObject->FileName.Buffer,
                  IrpSp->FileObject->FileName.Length);

    ADSPtr = wcschr(FileNameQuery, L':');

    if (ADSPtr)
    {
        /* This file request is for an alternate data stream.
         * Format:
         *     filename.ext:AttributeName:$AttributeType
         * If the last element is missing, it is equivalent to
         *     filename.ext:AttributeName:$DATA
         */
        DPRINT1("Asking for alternate data stream!\n");

        // Go to the next character after the colon
        ADSPtr++;
        ADSTypePtr = wcschr(ADSPtr, L':');

        if (ADSTypePtr)
        {
            // TODO: Use $AttrDef file to look up this type.
            ADSTypePtr++;
            DPRINT1("ADSType is \"%S\"\n", ADSTypePtr);
            __debugbreak();
            delete FileCB;
            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
            return STATUS_NOT_IMPLEMENTED;
        }

        else
        {
            // Copy the stream name
            FileCB->RequestedStream = new(NonPagedPool) WCHAR[wcslen(ADSPtr) + 1];
            RtlCopyMemory(FileCB->RequestedStream,
                          ADSPtr,
                          wcslen(ADSPtr) * sizeof(WCHAR));
            FileCB->RequestedStream[wcslen(ADSPtr)] = L'\0';

            // No type specified, use $DATA
            FileCB->RequestedType = TypeData;
        }
    }

    else
    {
        // This is a normal file.
        FileCB->RequestedType = TypeData;
        FileCB->RequestedStream = NULL;
    }

    // Add pointer for file record
    FileCB->FileRec = CurrentFile;

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
