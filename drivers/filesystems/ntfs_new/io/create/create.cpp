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

#define GetDisposition(x) ((x >> 24) & 0xFF)
#define GetCreateOptions(x) (x & 0xFFFFFF)

/* FUNCTIONS ****************************************************************/
extern PDEVICE_OBJECT NtfsDiskFileSystemDeviceObject;

NTSTATUS
GetFileRecordNumber(_In_  PWCHAR FileName,
                    _In_  UINT FileNameLength,
                    _In_  PNTFSVolume Volume,
                    _Out_ PULONGLONG FileRecordNumber)
{
    NTSTATUS Status;
    PFileRecord CurrentFile;
    Directory* CurrentDirectory;
    PWCHAR CurrentElement, EndOfFileName;
    ULONGLONG CurrentFRN;

    // Let's start with the root directory, which is hardcoded.
    if (FileName[0] == L'\0' ||
        wcscmp(L"\\", FileName) == 0)
    {
        *FileRecordNumber = _Root;
        return STATUS_SUCCESS;
    }

    /* Every other file will be inside of the root directory.
     * Parse the file name to find the file record number.
     */
    CurrentElement = &FileName[1];
    EndOfFileName = FileName + (FileNameLength * sizeof(WCHAR));

    Volume->MasterFileTable->GetFileRecord(_Root, &CurrentFile);

    CurrentDirectory = new(PagedPool) Directory();
    Status = CurrentDirectory->LoadDirectory(CurrentFile);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get directory!\n");
        __debugbreak();
        return STATUS_NOT_FOUND;
    }

    while (CurrentElement)
    {
        // Is the element in the current Btree?
        Status = CurrentDirectory->FindNextFile(CurrentElement,
                                                wcschr(FileName, L'\\') ?
                                                (wcschr(FileName, L'\\') - FileName) :
                                                wcslen(FileName),
                                                &CurrentFRN);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("Failed to find: \"%S\"\n", CurrentElement);
            Status = STATUS_NOT_FOUND;
            goto cleanup;
        }

        // Proceed to next element in the string. Sets NextElement to NULL if we are done.
        CurrentElement = wcschr(CurrentElement, L'\\');
        if (CurrentElement)
        {
            CurrentElement++;
            if (CurrentElement[0] != L'\0')
            {
                // Destroy the old BTree and make a new one if we're going in another layer.
                delete CurrentDirectory;
                delete CurrentFile;

                Volume->MasterFileTable->GetFileRecord(CurrentFRN, &CurrentFile);
                CurrentDirectory = new(PagedPool) Directory();

                if (!NT_SUCCESS(CurrentDirectory->LoadDirectory(CurrentFile)))
                {
                    DPRINT1("Unable to get directory!\n");
                    Status = STATUS_NOT_FOUND;
                    goto cleanup;
                }
            }
            else
            {
                CurrentElement = NULL;
            }
        }
    }

    *FileRecordNumber = CurrentFRN;
    Status = STATUS_SUCCESS;

cleanup:
    if (CurrentDirectory)
        delete CurrentDirectory;
    if (CurrentFile)
        delete CurrentFile;
    return Status;
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
    NTSTATUS Status;
    PFILE_OBJECT FileObject;
    BOOLEAN PerformAccessChecks;
    PWCH FileName;
    FileRecord* CurrentFile;
    PAttribute Attr;
    PStandardInformationEx StdInfo;
    UINT8 Disposition;
    ULONG CreateOptions;

    ULONGLONG FileRecordNumber;

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
    Disposition = GetDisposition(IrpSp->Parameters.Create.Options);
    CreateOptions = GetCreateOptions(IrpSp->Parameters.Create.Options);

    // Determine if we should check access rights
    PerformAccessChecks = (Irp->RequestorMode == UserMode) ||
                          (IrpSp->Flags & SL_FORCE_ACCESS_CHECK);

    // Hack: Fail certain requests we aren't ready for
    if (Disposition == FILE_SUPERSEDE ||
        Disposition == FILE_CREATE ||
        Disposition == FILE_OVERWRITE ||
        Disposition == FILE_OVERWRITE_IF ||
        wcschr(FileName, L':'))
    {
        DPRINT1("Rejecting file open!\n");
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        return STATUS_NOT_IMPLEMENTED;
    }

    // Get the file record number
    Status = GetFileRecordNumber(FileName, IrpSp->FileObject->FileName.Length, VolCB->Volume, &FileRecordNumber);
    if (!NT_SUCCESS(Status))
    {
        // This isn't always an issue, but it isn't implemented yet.
        DPRINT1("File not found!\n");
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        return Status;
    }

    // TODO: Check if we have rights to access file here.

    // Create file context block.
    FileCB = new(PagedPool) FileContextBlock();
    RtlZeroMemory(FileCB, sizeof(FileContextBlock));

    // Set file name.
    RtlCopyMemory(FileCB->FileName,
                  IrpSp->FileObject->FileName.Buffer,
                  IrpSp->FileObject->FileName.Length);

    FileCB->FileRecordNumber = FileRecordNumber;
    VolCB->Volume->MasterFileTable->GetFileRecord(FileRecordNumber, &CurrentFile);

    // From file record
    FileCB->NumberOfLinks = CurrentFile->Header->HardLinkCount;
    FileCB->IsDirectory = !!(CurrentFile->Header->Flags & FR_IS_DIRECTORY);

    // From $STANDARD_INFORMATION
    Attr = CurrentFile->GetAttribute(TypeStandardInformation,
                                     NULL);
    StdInfo = (PStandardInformationEx)GetResidentDataPointer(Attr);
    FileCB->CreationTime.QuadPart = StdInfo->CreationTime;
    FileCB->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
    FileCB->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
    FileCB->ChangeTime.QuadPart = StdInfo->ChangeTime;
    FileCB->FileAttributes = StdInfo->FilePermissions;

    // From $DATA
    Attr = CurrentFile->GetAttribute(TypeData,
                                     NULL);
    if (Attr)
    {
        if (Attr->IsNonResident)
            FileCB->EndOfFile.QuadPart = Attr->NonResident.DataSize;
        else
            FileCB->EndOfFile.QuadPart = Attr->Resident.DataLength;
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

    if (FileCB->IsDirectory)
    {
        // Set up btree for this file
        FileCB->FileDir = new(PagedPool) Directory();
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
