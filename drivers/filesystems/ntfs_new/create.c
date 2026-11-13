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

_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
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
    PNtfsFileRecord CurrentFile;
    UINT8 Disposition;
    PNtfsVolume DiskVolume;
    PNtfsMasterFileTable Mft;
    USHORT FileNameLength;
    BOOLEAN IsVolume;
    ULONG Information = 0;

    if (VolumeDeviceObject == NtfsDiskFileSystemDeviceObject)
    {
        /* DeviceObject represents FileSystem instead of logical volume */
        DPRINT1("Opening file system\n");
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = FILE_OPENED;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
        return STATUS_SUCCESS;
    }

    // Investigate file request
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileObject = IrpSp->FileObject;
    Disposition = GetDisposition(IrpSp->Parameters.Create.Options);
    DiskVolume = ((PVolumeContextBlock)VolumeDeviceObject->DeviceExtension)->DiskVolume;
    IsVolume = FileObject->FileName.Length == 0 && FileObject->RelatedFileObject == NULL;
    if (IsVolume)
    {
        if (Disposition != FILE_OPEN && Disposition != FILE_OPEN_IF)
        {
            Status = STATUS_ACCESS_DENIED;
            goto Done;
        }

        if (IrpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
        {
            Status = STATUS_NOT_A_DIRECTORY;
            goto Done;
        }

        if (IrpSp->Parameters.Create.Options & (FILE_DELETE_ON_CLOSE | FILE_OPEN_BY_FILE_ID))
        {
            Status = STATUS_INVALID_PARAMETER;
            goto Done;
        }

        goto InitializeFcb;
    }

    Mft = NtfsVolumeGetMft(DiskVolume);

    // Determine if we should check access rights
    PerformAccessChecks = (Irp->RequestorMode == UserMode) ||
                          (IrpSp->Flags & SL_FORCE_ACCESS_CHECK);

    // TODO: Check if we have rights to access file.

    // Try to find the requested file record.
    Status = NtfsMasterFileTableGetFileRecordFromQuery(Mft,
                                                       FileObject->FileName.Buffer,
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
                Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                return STATUS_NOT_IMPLEMENTED;
                break;
            case FILE_OPEN:
            case FILE_OVERWRITE:
            default:
                // In these cases, return an error.
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                return STATUS_INVALID_PARAMETER;
                break;
        }

    }

InitializeFcb:
    // Create file context block.
    FileCB = (PFileContextBlock)ExAllocatePoolZero(NonPagedPool,
                                                   sizeof(FileContextBlock),
                                                   TAG_NTFS);
    if (!FileCB)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    // Initialize the NT required FCB header and resources
    Status = ExInitializeResourceLite(&FileCB->MainResource);
    if (!NT_SUCCESS(Status))
    {
        ExFreePool(FileCB);
        goto Done;
    }

    Status = ExInitializeResourceLite(&FileCB->PagingIoResource);
    if (!NT_SUCCESS(Status))
    {
        ExDeleteResourceLite(&FileCB->MainResource);
        ExFreePool(FileCB);
        goto Done;
    }

    FsRtlInitializeFileLock(&FileCB->FileLock, NULL, NULL);
    ExInitializeFastMutex(&FileCB->HeaderMutex);
    FsRtlSetupAdvancedHeader(&FileCB->CommonFCBHeader, &FileCB->HeaderMutex);
    FileCB->CommonFCBHeader.Resource = &FileCB->MainResource;
    FileCB->CommonFCBHeader.PagingIoResource = &FileCB->PagingIoResource;
    FileCB->CommonFCBHeader.IsFastIoPossible = FastIoIsPossible;
    FileCB->CreateOptions = IrpSp->Parameters.Create.Options;
    FileCB->DesiredAccess = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;
    FileObject->SectionObjectPointer = &FileCB->SectionObjectPointers;
    RtlZeroMemory(&FileCB->SectionObjectPointers, sizeof(SECTION_OBJECT_POINTERS));

    if (IsVolume)
    {
        LARGE_INTEGER VolumeSize;
        VolumeSize.QuadPart = NtfsVolumeGetSize(DiskVolume);
        FileCB->CommonFCBHeader.IsFastIoPossible = FastIoIsNotPossible;
        FileCB->CommonFCBHeader.AllocationSize = VolumeSize;
        FileCB->CommonFCBHeader.FileSize = VolumeSize;
        FileCB->CommonFCBHeader.ValidDataLength = VolumeSize;
        FileCB->IsVolume = TRUE;
        FileObject->Flags |= FO_NO_INTERMEDIATE_BUFFERING;
        goto OpenFile;
    }

    // Set file name
    FileNameLength = IrpSp->FileObject->FileName.Length;

    PWCHAR FileNameBuffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool,
                                                          FileNameLength * sizeof(WCHAR),
                                                          TAG_NTFS);
    if (!FileNameBuffer)
        __debugbreak();
    RtlCopyMemory(FileNameBuffer,
                  IrpSp->FileObject->FileName.Buffer,
                  FileNameLength);
    FileCB->FileName.Buffer = FileNameBuffer;
    FileCB->FileName.Length = FileNameLength;
    FileCB->FileName.MaximumLength = FileNameLength;
    // NOTE: FileNameBuffer gets freed when the FileCB is cleaned up.

    // Get ADS Preferences for the file.
    Status = NtfsVolumeGetADSPreference(DiskVolume,
                                        FileObject,
                                        &FileCB->RequestedType,
                                        &FileCB->RequestedStream);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get ADS preference! Aborting...\n");
        FileObject->SectionObjectPointer = NULL;
        ExFreePool(FileCB);
        Irp->IoStatus.Status = Status;
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
        return Status;
    }

    FileCB->FileRec = CurrentFile;

    /* Assume that this is the first file stream request.
     * For more details see:
     * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_section_object_pointers
     *
     * TODO: Handle multiple opened files pointing to the same stream properly.
     */
    FileCB->StreamCB = (StreamContextBlock*)ExAllocatePoolWithTag(NonPagedPool,
                                                                  sizeof(StreamContextBlock),
                                                                  TAG_NTFS);
    if (!FileCB->StreamCB)
        __debugbreak();

    RtlZeroMemory(&FileCB->StreamCB->SectionObjectPointers,
                  sizeof(SECTION_OBJECT_POINTERS));

    if (!!(NtfsFileRecordGetHeader(CurrentFile)->Flags & FR_IS_DIRECTORY))
    {
        // Set up btree for this file
        FileCB->FileDir = NtfsDirectoryCreate(DiskVolume);
        Status = NtfsDirectoryLoadDirectory(FileCB->FileDir, FileCB->FileRec);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to get directory!\n");
            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
            return Status;
        }
    }

    // Initialize cache map on first open when we have valid sizes
    {
        // Use the CommonFCBHeader fields as the canonical CC_FILE_SIZES storage
        PCC_FILE_SIZES FileSizes = (PCC_FILE_SIZES)&FileCB->CommonFCBHeader.AllocationSize;
        // Initialize the common header sizes from attributes
        PAttribute DataAttr = NtfsFileRecordGetAttribute(CurrentFile, TypeData, NULL);
        if (DataAttr)
        {
            if (DataAttr->IsNonResident)
            {
                FileCB->CommonFCBHeader.AllocationSize.QuadPart = DataAttr->NonResident.AllocatedSize;
                FileCB->CommonFCBHeader.FileSize.QuadPart       = DataAttr->NonResident.DataSize;
                FileCB->CommonFCBHeader.ValidDataLength.QuadPart= DataAttr->NonResident.InitalizedDataSize;
            }
            else
            {
                FileCB->CommonFCBHeader.AllocationSize.QuadPart = 0;
                FileCB->CommonFCBHeader.FileSize.QuadPart       = DataAttr->Resident.DataLength;
                FileCB->CommonFCBHeader.ValidDataLength.QuadPart= DataAttr->Resident.DataLength;
            }
        }
        else
        {
            FileCB->CommonFCBHeader.AllocationSize.QuadPart = 0;
            FileCB->CommonFCBHeader.FileSize.QuadPart       = 0;
            FileCB->CommonFCBHeader.ValidDataLength.QuadPart= 0;
        }

        // Build callbacks for Cache Manager
        CACHE_MANAGER_CALLBACKS Callbacks = {0};
        Callbacks.AcquireForLazyWrite  = NtfsAcqLazyWrite;
        Callbacks.ReleaseFromLazyWrite = NtfsRelLazyWrite;
        Callbacks.AcquireForReadAhead  = NtfsAcqReadAhead;
        Callbacks.ReleaseFromReadAhead = NtfsRelReadAhead;

        // Initialize cache map
        if (FileObject->PrivateCacheMap == NULL)
        {
            CcInitializeCacheMap(FileObject,
                                 FileSizes,
                                 FALSE,
                                 &Callbacks,
                                 FileCB);
            CcSetFileSizes(FileObject, FileSizes);
            CcSetReadAheadGranularity(FileObject, 0x10000);
            FileObject->Flags |= FO_CACHE_SUPPORTED;
        }
    }

OpenFile:
    // Set FsContext to the file context block and open file.
    FileObject->FsContext = FileCB;
    Status = STATUS_SUCCESS;
    Information = FILE_OPENED;

Done:
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
