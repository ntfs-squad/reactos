/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new file information
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

 static
 NTSTATUS
 GetFileBasicInformation(_In_ PFileContextBlock FileCB,
                         _Out_ PFILE_BASIC_INFORMATION Buffer,
                         _Inout_ PULONG Length)
 {
     PFileRecord File;
     PStandardInformationEx StdInfo;
 
     if (!FileCB)
         return STATUS_INVALID_PARAMETER;
 
     if (*Length < sizeof(FILE_BASIC_INFORMATION))
         return STATUS_BUFFER_TOO_SMALL;
 
     File = FileCB->FileRec;
 
     // From $STANDARD_INFORMATION
     {
         PAttribute StdAttr = File->GetAttribute(TypeStandardInformation, NULL);
         if (!StdAttr || StdAttr->IsNonResident)
             return STATUS_FILE_CORRUPT_ERROR;
         StdInfo = (PStandardInformationEx) GetResidentDataPointer(StdAttr);
     }
 
     Buffer->CreationTime.QuadPart = StdInfo->CreationTime;
     Buffer->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
     Buffer->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
     Buffer->ChangeTime.QuadPart = StdInfo->ChangeTime;
     Buffer->FileAttributes = StdInfo->FilePermissions;
 
     *Length -= sizeof(FILE_BASIC_INFORMATION);
 
     return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 GetFileStandardInformation(_In_ PFileContextBlock FileCB,
                            _Out_ PFILE_STANDARD_INFORMATION Buffer,
                            _Inout_ PULONG Length)
 {
     PFileRecord File;
     PAttribute DataAttribute;
     size_t FileInfoSize = sizeof(FILE_STANDARD_INFORMATION);
 
     if (*Length < FileInfoSize)
         return STATUS_BUFFER_TOO_SMALL;
 
     if (!FileCB)
         return STATUS_NOT_FOUND;
 
     File = FileCB->FileRec;
 
     // Information from $DATA
     DataAttribute = File->GetAttribute(TypeData,
                                        NULL);
 
     if (DataAttribute)
     {
         if (DataAttribute->IsNonResident)
         {
             Buffer->EndOfFile.QuadPart = DataAttribute->NonResident.DataSize;
             Buffer->AllocationSize.QuadPart = DataAttribute->NonResident.AllocatedSize;
         }
 
         else
         {
             Buffer->EndOfFile.QuadPart = DataAttribute->Resident.DataLength;
             Buffer->AllocationSize.QuadPart = 0;
         }
     }
 
     else
     {
         Buffer->EndOfFile.QuadPart = 0;
         Buffer->AllocationSize.QuadPart = 0;
     }
 
     // Information from file header
     Buffer->Directory = !!(File->Header->Flags & FR_IS_DIRECTORY);
     Buffer->NumberOfLinks = File->Header->HardLinkCount;
 
     // Information from file context block
     Buffer->DeletePending = !!(FileCB->CreateOptions & FILE_DELETE_ON_CLOSE);
 
     *Length -= FileInfoSize;
 
     return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 GetFileNameInformation(_In_ PFileContextBlock FileCB,
                        _Out_ PFILE_NAME_INFORMATION Buffer,
                        _Inout_ PULONG Length)
 {
     ULONG BytesToCopy;
     size_t FileNameInfoSize = sizeof(FILE_NAME_INFORMATION);
 
     // If buffer can't hold the File Name Information struct, fail and
     // report the required size back to caller.
     if (*Length < FileNameInfoSize)
     {
         *Length = (ULONG)(FileNameInfoSize + FileCB->FileName.Length);
         return STATUS_INFO_LENGTH_MISMATCH;
     }
 
     // Save file name length, and as much file len, as buffer length allows.
     Buffer->FileNameLength = FileCB->FileName.Length;
     // Calculate amount of bytes to copy not to overflow the buffer.
     // TODO: Determine if we need this
     if (*Length < Buffer->FileNameLength + FileNameInfoSize)
     {
         // The buffer isn't big enough. Report required size.
         *Length = (ULONG)(FileNameInfoSize + Buffer->FileNameLength);
         return STATUS_BUFFER_OVERFLOW;
     }
 
     else
     {
         // The buffer is big enough. Fill with file name.
         BytesToCopy = Buffer->FileNameLength;
         RtlCopyMemory(Buffer->FileName, FileCB->FileName.Buffer, BytesToCopy);
         *Length -= FileNameInfoSize + BytesToCopy;
         return STATUS_SUCCESS;
     }
 }
 
 static
 NTSTATUS
 GetFileInternalInformation(_In_ PFileContextBlock FileCB,
                            _Out_ PFILE_INTERNAL_INFORMATION Buffer,
                            _Inout_ PULONG Length)
 {
 
     /* From Microsoft Learn:
      * The FILE_INTERNAL_INFORMATION structure is used to query for the file
      * system's 8-byte file reference number for a file.
      */
 
     if (*Length < sizeof(FILE_INTERNAL_INFORMATION))
         return STATUS_BUFFER_TOO_SMALL;
 
     Buffer->IndexNumber.QuadPart = FileCB->FileRec->Header->MFTRecordNumber;
 
     *Length -= sizeof(FILE_INTERNAL_INFORMATION);
 
     return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 GetFileNetworkOpenInformation(_In_ PFileContextBlock FileCB,
                               _Out_ PFILE_NETWORK_OPEN_INFORMATION Buffer,
                               _Inout_ PULONG Length)
 {
     PAttribute DataAttribute;
     PStandardInformationEx StdInfo;
     PFileRecord File;
 
     ASSERT(Buffer);
     ASSERT(FileCB);
 
     File = FileCB->FileRec;
 
     // Information from $DATA
     DataAttribute = File->GetAttribute(TypeData,
                                        NULL);
 
     if (DataAttribute)
     {
         if (DataAttribute->IsNonResident)
         {
             Buffer->EndOfFile.QuadPart = DataAttribute->NonResident.DataSize;
             Buffer->AllocationSize.QuadPart = DataAttribute->NonResident.AllocatedSize;
         }
 
         else
         {
             Buffer->EndOfFile.QuadPart = DataAttribute->Resident.DataLength;
             Buffer->AllocationSize.QuadPart = 0;
         }
     }
 
     else
     {
         Buffer->EndOfFile.QuadPart = 0;
         Buffer->AllocationSize.QuadPart = 0;
     }
 
     File = FileCB->FileRec;
 
     // From $STANDARD_INFORMATION
     {
         PAttribute StdAttr = File->GetAttribute(TypeStandardInformation, NULL);
         if (!StdAttr || StdAttr->IsNonResident)
             return STATUS_FILE_CORRUPT_ERROR;
         StdInfo = (PStandardInformationEx) GetResidentDataPointer(StdAttr);
     }
 
     Buffer->CreationTime.QuadPart = StdInfo->CreationTime;
     Buffer->LastAccessTime.QuadPart = StdInfo->LastAccessTime;
     Buffer->LastWriteTime.QuadPart = StdInfo->LastWriteTime;
     Buffer->ChangeTime.QuadPart = StdInfo->ChangeTime;
     Buffer->FileAttributes = StdInfo->FilePermissions;
 
     *Length -= (sizeof(PFILE_NETWORK_OPEN_INFORMATION));
 
     return STATUS_SUCCESS;
 }

static
inline
BOOLEAN
ContainsWildcard(PUNICODE_STRING String)
{
    USHORT charCount = (USHORT)(String->Length / sizeof(WCHAR));
    for (USHORT i = 0; i < charCount; i++)
    {
        if (String->Buffer[i] == L'*'    ||
            String->Buffer[i] == L'?'    ||
            String->Buffer[i] == DOS_DOT ||
            String->Buffer[i] == DOS_QM  ||
            String->Buffer[i] == DOS_STAR)
            return TRUE;
    }
    return FALSE;
}

static
NTSTATUS
GetFileBothDirectoryInformation(_In_    PFileContextBlock FileCB,
                                _In_    UCHAR IrpFlags,
                                _In_    PUNICODE_STRING FileNameFilter,
                                _Out_   PFILE_BOTH_DIR_INFORMATION Buffer,
                                _Inout_ PULONG Length)
{
    Directory* FileDir;
    BOOLEAN ReturnSingleEntry, RestartScan;

    if (!FileCB)
    {
        DPRINT1("INVESTIGATE ME: GetFileBothDirectoryInformation() called with NULL FileCB!\n");
        return STATUS_INVALID_PARAMETER;
    }

    FileDir = FileCB->FileDir;
    RestartScan = !!(IrpFlags & SL_RESTART_SCAN);

    if (!FileDir)
    {
        DPRINT1("This is not a directory!\n");
        return STATUS_NOT_FOUND;
    }

    /* If there's no wild cards and a file name filter
     * is specified, we will only return one entry.
     */
    if (FileNameFilter &&
        !ContainsWildcard(FileNameFilter))
        ReturnSingleEntry = TRUE;
    else
        ReturnSingleEntry = !!(IrpFlags & SL_RETURN_SINGLE_ENTRY);

    return FileDir->GetFileBothDirInfo(ReturnSingleEntry,
                                       RestartScan,
                                       FileNameFilter,
                                       Buffer,
                                       Length);
}

/* GLOBALS *****************************************************************/

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsFsdQueryInformation)
#pragma alloc_text(PAGE, NtfsFsdSetInformation)
#pragma alloc_text(PAGE, NtfsFsdDirectoryControl)
#endif

/* FUNCTIONS ****************************************************************/

_Function_class_(IRP_MJ_QUERY_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp)
{

    /* Overview:
     * Determine if file information request is appropriate.
     * If it is, fulfill it. If it isn't, return STATUS_INVALID_DEVICE_REQUEST.
     *
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mj-query-information
     */

    PIO_STACK_LOCATION IoStack;
    FILE_INFORMATION_CLASS FileInfoRequest;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    NTSTATUS Status;
    PVOID SystemBuffer;
    PFILE_OBJECT FileObject;
    ULONG BufferLength;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    FileInfoRequest = IoStack->Parameters.QueryFile.FileInformationClass;
    FileObject = IoStack->FileObject;
    VolCB = (PVolumeContextBlock)VolumeDeviceObject->DeviceExtension;
    FileCB = (PFileContextBlock)FileObject->FsContext;
    if (!FileCB)
    {
        DPRINT1("NtfsFsdQueryInformation() called with NULL file context block!\n");
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto Done;
    }

    FileObject->SectionObjectPointer = &(FileCB->StreamCB->SectionObjectPointers);
    SystemBuffer = GetBuffer(Irp);
    BufferLength = IoStack->Parameters.QueryFile.Length;

    switch (FileInfoRequest)
    {
        case FileBasicInformation:
            Status = GetFileBasicInformation(FileCB,
                                             (PFILE_BASIC_INFORMATION)SystemBuffer,
                                             &BufferLength);
            break;
        case FileStandardInformation:
            Status = GetFileStandardInformation(FileCB,
                                                (PFILE_STANDARD_INFORMATION)SystemBuffer,
                                                &BufferLength);
            break;
        case FileInternalInformation:
            Status = GetFileInternalInformation(FileCB,
                                                (PFILE_INTERNAL_INFORMATION)SystemBuffer,
                                                &BufferLength);
            break;
        case FileNameInformation:
            Status = GetFileNameInformation(FileCB,
                                            (PFILE_NAME_INFORMATION)SystemBuffer,
                                            &BufferLength);
            break;
        case FileNetworkOpenInformation:
            Status = GetFileNetworkOpenInformation(FileCB,
                                                   (PFILE_NETWORK_OPEN_INFORMATION)SystemBuffer,
                                                   &BufferLength);
            break;
        default:
            DPRINT1("Unhandled file information request %d!\n", FileInfoRequest);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

Done:
    if (NT_SUCCESS(Status))
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = IoStack->Parameters.QueryFile.Length - BufferLength;

#ifdef __REACTOS__
        // HACK!!! Driver should not have to edit UserIosb.
        Irp->UserIosb->Status = Irp->IoStatus.Status;
        Irp->UserIosb->Information = Irp->IoStatus.Information;
#endif
    }
    else
    {
        Irp->IoStatus.Information = 0;
    }

    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}

_Function_class_(IRP_MJ_SET_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp)
{
    /* Overview:
     * Check if a requested file is open.
     * If it is, set information in the file as requested.
     *
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-set-information
     */
    DPRINT1("NtfsFsdSetInformation Called!\n");
    return 0;
}

_Function_class_(IRP_MJ_DIRECTORY_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDirectoryControl(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp)
{
    /* Overview:
     * See: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-directory-control
     */

    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    FILE_INFORMATION_CLASS FileInformationRequest;
    PVolumeContextBlock VolCB;
    PFileContextBlock FileCB;
    PVOID SystemBuffer;
    ULONG BufferLength;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileCB = (PFileContextBlock)(IrpSp->FileObject->FsContext);
    VolCB = (PVolumeContextBlock)(VolumeDeviceObject->DeviceExtension);
    SystemBuffer = GetBuffer(Irp);
    BufferLength = IrpSp->Parameters.QueryDirectory.Length;

    if (!SystemBuffer)
    {
        Status = STATUS_INVALID_USER_BUFFER;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
        return Status;
    }

    if (IrpSp->MinorFunction == IRP_MN_QUERY_DIRECTORY)
    {
        FileInformationRequest = IrpSp->Parameters.QueryDirectory.FileInformationClass;

        switch(FileInformationRequest)
        {
            case FileBothDirectoryInformation:
                Status = GetFileBothDirectoryInformation(FileCB,
                                                         IrpSp->Flags,
                                                         IrpSp->Parameters.QueryDirectory.FileName,
                                                         (PFILE_BOTH_DIR_INFORMATION)SystemBuffer,
                                                         &BufferLength);
                break;
            case FileDirectoryInformation:
                DPRINT1("FileDirectoryInformation request!\n");
                break;
            case FileFullDirectoryInformation:
                DPRINT1("FileFullDirectoryInformation request!\n");
                break;
            case FileIdBothDirectoryInformation:
                DPRINT1("FileIdBothDirectoryInformation request!\n");
                break;
            case FileIdFullDirectoryInformation:
                DPRINT1("FileIdFullDirectoryInformation request!\n");
                break;
            case FileNamesInformation:
                DPRINT1("FileNamesInformation request!\n");
                break;
            case FileObjectIdInformation:
                DPRINT1("FileObjectIdInformation request!\n");
                break;
            case FileReparsePointInformation:
                DPRINT1("FileReparsePointInformation request!\n");
                break;
            default:
                DPRINT1("Unknown directory query request! %lu\n", FileInformationRequest);
                break;
        }
    }

    else
    {
        if (IrpSp->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY)
        {
            DPRINT1("IRP_MN_NOTIFY_CHANGE_DIRECTORY\n");
            Status = STATUS_SUCCESS;
        }

        else
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    // Set to number of bytes written
    if (NT_SUCCESS(Status))
    {
        Irp->IoStatus.Information = IrpSp->Parameters.QueryDirectory.Length - BufferLength;
    }
    else
    {
        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}
