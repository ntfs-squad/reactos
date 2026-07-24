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
    NTSTATUS Status;
    NtfsFileBasicInformation Information;
 
    if (!FileCB)
        return STATUS_INVALID_PARAMETER;
 
    if (*Length < sizeof(FILE_BASIC_INFORMATION))
        return STATUS_BUFFER_TOO_SMALL;
 
    Status = NtfsFileRecordGetBasicInformation(
        FileCB->FileRec,
        &Information);
    if (!NT_SUCCESS(Status))
        return Status;
     
    Buffer->CreationTime.QuadPart =
        Information.CreationTime;
    Buffer->LastAccessTime.QuadPart =
        Information.LastAccessTime;
    Buffer->LastWriteTime.QuadPart =
        Information.LastWriteTime;
    Buffer->ChangeTime.QuadPart =
        Information.ChangeTime;
    Buffer->FileAttributes =
        Information.FileAttributes;
 
    *Length -= sizeof(FILE_BASIC_INFORMATION);
 
    return STATUS_SUCCESS;
 }
 
 static
 NTSTATUS
 GetFileStandardInformation(_In_ PFileContextBlock FileCB,
                            _Out_ PFILE_STANDARD_INFORMATION Buffer,
                            _Inout_ PULONG Length)
 {
     PNtfsFileRecord File;
     PAttribute DataAttribute;
     size_t FileInfoSize = sizeof(FILE_STANDARD_INFORMATION);
 
     if (*Length < FileInfoSize)
         return STATUS_BUFFER_TOO_SMALL;
 
     if (!FileCB)
         return STATUS_NOT_FOUND;
 
     File = FileCB->FileRec;
 
     // Information from the stream represented by this file object.
     DataAttribute = NtfsFileRecordGetAttribute(
         File,
         FileCB->RequestedType,
         FileCB->RequestedStream);
 
     if (DataAttribute)
     {
         if (DataAttribute->IsNonResident)
         {
             Buffer->EndOfFile.QuadPart = DataAttribute->NonResident.DataSize;
             Buffer->AllocationSize.QuadPart =
                 NtfsAttributeGetPhysicalAllocationSize(
                     DataAttribute);
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
     Buffer->Directory = !!(NtfsFileRecordGetHeader(File)->Flags & FR_IS_DIRECTORY);
     Buffer->NumberOfLinks = NtfsFileRecordGetHeader(File)->HardLinkCount;
 
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
 
     Buffer->IndexNumber.QuadPart = NtfsFileRecordGetHeader(FileCB->FileRec)->MFTRecordNumber;
 
     *Length -= sizeof(FILE_INTERNAL_INFORMATION);
 
     return STATUS_SUCCESS;
 }

static
NTSTATUS
GetFileEaInformation(_In_ PFileContextBlock FileCB,
                     _Out_ PFILE_EA_INFORMATION Buffer,
                     _Inout_ PULONG Length)
{
    EAInformationEx EaInformation;
    ULONG EaLength = 0;
    NTSTATUS Status;

    if (!FileCB || !FileCB->FileRec)
        return STATUS_INVALID_PARAMETER;
    if (*Length < sizeof(*Buffer))
        return STATUS_BUFFER_TOO_SMALL;

    Status = NtfsFileRecordReadExtendedAttributes(FileCB->FileRec,
                                                   NULL,
                                                   &EaLength,
                                                   &EaInformation);
    if (Status == STATUS_NO_EAS_ON_FILE)
    {
        Buffer->EaSize = 0;
    }
    else if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        Buffer->EaSize = EaInformation.UnpackedEASize;
    }
    else
    {
        return Status;
    }

    *Length -= sizeof(*Buffer);
    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileNetworkOpenInformation(_In_ PFileContextBlock FileCB,
                              _Out_ PFILE_NETWORK_OPEN_INFORMATION Buffer,
                              _Inout_ PULONG Length)
{
    NTSTATUS Status;
    NtfsFileBasicInformation Information;
    PAttribute DataAttribute;
    PNtfsFileRecord File;

    ASSERT(Buffer);
    ASSERT(FileCB);

    File = FileCB->FileRec;

    // Information from the stream represented by this file object.
    DataAttribute = NtfsFileRecordGetAttribute(
        File,
        FileCB->RequestedType,
        FileCB->RequestedStream);

    if (DataAttribute)
    {
        if (DataAttribute->IsNonResident)
        {
            Buffer->EndOfFile.QuadPart = DataAttribute->NonResident.DataSize;
            Buffer->AllocationSize.QuadPart =
                NtfsAttributeGetPhysicalAllocationSize(
                    DataAttribute);
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

    Status = NtfsFileRecordGetBasicInformation(
        File,
        &Information);
    if (!NT_SUCCESS(Status))
        return Status;

    Buffer->CreationTime.QuadPart =
        Information.CreationTime;
    Buffer->LastAccessTime.QuadPart =
        Information.LastAccessTime;
    Buffer->LastWriteTime.QuadPart =
        Information.LastWriteTime;
    Buffer->ChangeTime.QuadPart =
        Information.ChangeTime;
    Buffer->FileAttributes =
        Information.FileAttributes;

    *Length -= sizeof(FILE_NETWORK_OPEN_INFORMATION);

    return STATUS_SUCCESS;
}

static
NTSTATUS
GetFileStreamInformation(
    _In_ PFileContextBlock FileCB,
    _Out_ PFILE_STREAM_INFORMATION Buffer,
    _Inout_ PULONG Length)
{
    static const WCHAR DataSuffix[] = L":$DATA";
    PNtfsDataStreamInformation Streams = NULL;
    PFILE_STREAM_INFORMATION Current;
    PFILE_STREAM_INFORMATION Last = NULL;
    ULONG Available;
    ULONG Capacity = 4;
    ULONG Count;
    ULONG BytesWritten = 0;
    ULONG LastOffset = 0;
    ULONG LastSize = 0;
    ULONG HeaderSize;
    ULONG Index;
    NTSTATUS Status;

    if (!FileCB || !FileCB->FileRec ||
        !Length)
    {
        return STATUS_INVALID_PARAMETER;
    }
    Available = *Length;
    if (Available < sizeof(FILE_STREAM_INFORMATION))
        return STATUS_INFO_LENGTH_MISMATCH;
    if (!Buffer)
        return STATUS_INVALID_USER_BUFFER;

    for (;;)
    {
        if (Capacity >
            MAXULONG / sizeof(*Streams))
        {
            Status = STATUS_FILE_TOO_LARGE;
            goto Done;
        }
        Streams = (PNtfsDataStreamInformation)
            ExAllocatePoolWithTag(
                PagedPool,
                Capacity * sizeof(*Streams),
                TAG_NTFS);
        if (!Streams)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Done;
        }

        Count = Capacity;
        Status = NtfsFileRecordQueryDataStreams(
            FileCB->FileRec,
            Streams,
            &Count);
        if (Status != STATUS_BUFFER_TOO_SMALL &&
            Status != STATUS_BUFFER_OVERFLOW)
        {
            break;
        }

        ExFreePoolWithTag(Streams, TAG_NTFS);
        Streams = NULL;
        if (Capacity > MAXULONG / 2)
        {
            Status = STATUS_FILE_TOO_LARGE;
            goto Done;
        }
        Capacity *= 2;
    }
    if (!NT_SUCCESS(Status))
        goto Done;

    HeaderSize = FIELD_OFFSET(
        FILE_STREAM_INFORMATION,
        StreamName);
    for (Index = 0; Index < Count; Index++)
    {
        ULONG NameBytes;
        ULONG ThisSize;
        ULONG EntrySize;

        if (Streams[Index].DataSize >
                MAXLONGLONG ||
            Streams[Index].AllocationSize >
                MAXLONGLONG)
        {
            Status = STATUS_FILE_TOO_LARGE;
            goto Done;
        }

        NameBytes =
            (Streams[Index].NameLength + 7) *
            sizeof(WCHAR);
        ThisSize = HeaderSize + NameBytes;
        EntrySize = Index + 1 == Count
            ? ThisSize
            : ALIGN_UP_BY(ThisSize,
                          sizeof(ULONGLONG));
        if (BytesWritten > Available ||
            ThisSize >
                Available - BytesWritten)
        {
            if (Last)
            {
                Last->NextEntryOffset = 0;
                BytesWritten =
                    LastOffset + LastSize;
            }
            Status = STATUS_BUFFER_OVERFLOW;
            goto Done;
        }

        Current = (PFILE_STREAM_INFORMATION)
            ((PUCHAR)Buffer + BytesWritten);
        RtlZeroMemory(Current, EntrySize);
        Current->NextEntryOffset =
            Index + 1 == Count ? 0 : EntrySize;
        Current->StreamNameLength = NameBytes;
        Current->StreamSize.QuadPart =
            (LONGLONG)Streams[Index].DataSize;
        Current->StreamAllocationSize.QuadPart =
            (LONGLONG)
                Streams[Index].AllocationSize;
        Current->StreamName[0] = L':';
        if (Streams[Index].NameLength != 0)
        {
            RtlCopyMemory(
                Current->StreamName + 1,
                Streams[Index].Name,
                Streams[Index].NameLength *
                    sizeof(WCHAR));
        }
        RtlCopyMemory(
            Current->StreamName + 1 +
                Streams[Index].NameLength,
            DataSuffix,
            (RTL_NUMBER_OF(DataSuffix) - 1) *
                sizeof(WCHAR));

        Last = Current;
        LastOffset = BytesWritten;
        LastSize = ThisSize;
        BytesWritten += ThisSize;
        if (Index + 1 != Count)
        {
            if (EntrySize >
                Available - LastOffset)
            {
                Current->NextEntryOffset = 0;
                Status = STATUS_BUFFER_OVERFLOW;
                goto Done;
            }
            BytesWritten =
                LastOffset + EntrySize;
        }
    }
    Status = STATUS_SUCCESS;

Done:
    if (Streams)
        ExFreePoolWithTag(Streams, TAG_NTFS);
    *Length = Available - BytesWritten;
    return Status;
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
    PNtfsDirectory FileDir;
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

    return NtfsDirectoryGetFileBothDirInfo(FileDir,
                                           ReturnSingleEntry,
                                           RestartScan,
                                           FileNameFilter,
                                           Buffer,
                                           Length);
 }

VOID
NtfsRefreshFileSizes(_In_ PFileContextBlock FileCB,
                     _In_opt_ PFILE_OBJECT FileObject)
{
    PAttribute DataAttribute;

    if (!FileCB || !FileCB->FileRec)
        return;

    DataAttribute = NtfsFileRecordGetAttribute(
        FileCB->FileRec,
        FileCB->RequestedType,
        FileCB->RequestedStream);
    if (DataAttribute)
    {
        if (DataAttribute->IsNonResident)
        {
            FileCB->CommonFCBHeader.AllocationSize.QuadPart =
                NtfsAttributeGetPhysicalAllocationSize(
                    DataAttribute);
            FileCB->CommonFCBHeader.FileSize.QuadPart =
                DataAttribute->NonResident.DataSize;
            FileCB->CommonFCBHeader.ValidDataLength.QuadPart =
                DataAttribute->NonResident.InitalizedDataSize;
        }
        else
        {
            FileCB->CommonFCBHeader.AllocationSize.QuadPart = 0;
            FileCB->CommonFCBHeader.FileSize.QuadPart =
                DataAttribute->Resident.DataLength;
            FileCB->CommonFCBHeader.ValidDataLength.QuadPart =
                DataAttribute->Resident.DataLength;
        }
    }
    else
    {
        FileCB->CommonFCBHeader.AllocationSize.QuadPart = 0;
        FileCB->CommonFCBHeader.FileSize.QuadPart = 0;
        FileCB->CommonFCBHeader.ValidDataLength.QuadPart = 0;
    }

    if (FileObject && CcIsFileCached(FileObject))
    {
        CcSetFileSizes(
            FileObject,
            (PCC_FILE_SIZES)&
                FileCB->CommonFCBHeader.AllocationSize);
    }
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
        case FileEaInformation:
            Status = GetFileEaInformation(FileCB,
                                          (PFILE_EA_INFORMATION)SystemBuffer,
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
        case FileStreamInformation:
            Status = GetFileStreamInformation(
                FileCB,
                (PFILE_STREAM_INFORMATION)SystemBuffer,
                &BufferLength);
            break;
        default:
            DPRINT1("Unhandled file information request %d!\n", FileInfoRequest);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

Done:
    Irp->IoStatus.Status = Status;
    if (NT_SUCCESS(Status) ||
        Status == STATUS_BUFFER_OVERFLOW)
    {
        Irp->IoStatus.Information =
            IoStack->Parameters.QueryFile.Length -
            BufferLength;

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
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_ PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp)
{
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;
    PFileContextBlock FileCB;
    PVolumeContextBlock VolCB;
    PVOID SystemBuffer;
    LARGE_INTEGER RequestedSize;
    NtfsFileBasicInformation BasicInformation;
    PFILE_BASIC_INFORMATION RequestedBasic;
    ULONG NewTimestampMask;
    ULONG OldTimestampMask;
    ULONG BufferLength;
    NTSTATUS Status;
    BOOLEAN AllocationRequest = FALSE;
    BOOLEAN ResourceAcquired = FALSE;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileObject = IrpSp->FileObject;
    FileCB = FileObject
        ? (PFileContextBlock)FileObject->FsContext
        : NULL;
    VolCB = VolumeDeviceObject
        ? (PVolumeContextBlock)
            VolumeDeviceObject->DeviceExtension
        : NULL;
    SystemBuffer = GetBuffer(Irp);
    BufferLength =
        IrpSp->Parameters.SetFile.Length;

    if (!FileObject || !FileCB || !FileCB->FileRec ||
        !VolCB || !VolCB->DiskVolume)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Complete;
    }
    if (NtfsVolumeIsReadOnly(VolCB->DiskVolume))
    {
        Status = STATUS_MEDIA_WRITE_PROTECTED;
        goto Complete;
    }
    if (!SystemBuffer)
    {
        Status = STATUS_INVALID_USER_BUFFER;
        goto Complete;
    }

    ExAcquireResourceExclusiveLite(
        &FileCB->MainResource,
        TRUE);
    ResourceAcquired = TRUE;

    switch (IrpSp->Parameters.SetFile.
                FileInformationClass)
    {
        case FileBasicInformation:
            if (BufferLength <
                sizeof(FILE_BASIC_INFORMATION))
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                goto Complete;
            }
            if (!(FileCB->DesiredAccess &
                  FILE_WRITE_ATTRIBUTES))
            {
                Status = STATUS_ACCESS_DENIED;
                goto Complete;
            }

            RequestedBasic =
                (PFILE_BASIC_INFORMATION)SystemBuffer;
            RtlZeroMemory(&BasicInformation,
                          sizeof(BasicInformation));
            OldTimestampMask =
                FileCB->AutomaticTimestampMask;
            NewTimestampMask = OldTimestampMask;

#define SET_BASIC_TIME(Member, Mask, AutomaticMask) \
    do { \
        if (RequestedBasic->Member.QuadPart > 0) \
        { \
            BasicInformation.Fields |= (Mask); \
            BasicInformation.Member = \
                (ULONGLONG)RequestedBasic->Member.QuadPart; \
            NewTimestampMask &= ~(AutomaticMask); \
        } \
        else if (RequestedBasic->Member.QuadPart == -1) \
        { \
            NewTimestampMask &= ~(AutomaticMask); \
        } \
        else if (RequestedBasic->Member.QuadPart == -2) \
        { \
            NewTimestampMask |= (AutomaticMask); \
        } \
        else if (RequestedBasic->Member.QuadPart != 0) \
        { \
            Status = STATUS_INVALID_PARAMETER; \
            goto Complete; \
        } \
    } while (0)

            SET_BASIC_TIME(
                CreationTime,
                NTFS_BASIC_INFO_CREATION_TIME,
                0);
            SET_BASIC_TIME(
                LastAccessTime,
                NTFS_BASIC_INFO_LAST_ACCESS_TIME,
                NTFS_BASIC_INFO_LAST_ACCESS_TIME);
            SET_BASIC_TIME(
                LastWriteTime,
                NTFS_BASIC_INFO_LAST_WRITE_TIME,
                NTFS_BASIC_INFO_LAST_WRITE_TIME);
            SET_BASIC_TIME(
                ChangeTime,
                NTFS_BASIC_INFO_CHANGE_TIME,
                NTFS_BASIC_INFO_CHANGE_TIME);
#undef SET_BASIC_TIME

            if (RequestedBasic->FileAttributes != 0)
            {
                BasicInformation.Fields |=
                    NTFS_BASIC_INFO_FILE_ATTRIBUTES;
                BasicInformation.FileAttributes =
                    RequestedBasic->FileAttributes;
            }

            Status =
                NtfsFileRecordSetAutomaticTimestampMask(
                    FileCB->FileRec,
                    NewTimestampMask);
            if (!NT_SUCCESS(Status))
                goto Complete;

            Status =
                NtfsFileRecordSetBasicInformation(
                    FileCB->FileRec,
                    &BasicInformation);
            if (NT_SUCCESS(Status))
            {
                FileCB->AutomaticTimestampMask =
                    NewTimestampMask;
            }
            else
            {
                (void)
                    NtfsFileRecordSetAutomaticTimestampMask(
                        FileCB->FileRec,
                        OldTimestampMask);
            }
            if (NT_SUCCESS(Status) &&
                BasicInformation.Fields != 0)
            {
                FileObject->Flags |= FO_FILE_MODIFIED;
            }
            goto Complete;

        case FileEndOfFileInformation:
            if (BufferLength <
                sizeof(FILE_END_OF_FILE_INFORMATION))
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                goto Complete;
            }
            if (IrpSp->Parameters.SetFile.AdvanceOnly)
            {
                Status = STATUS_NOT_IMPLEMENTED;
                goto Complete;
            }
            RequestedSize =
                ((PFILE_END_OF_FILE_INFORMATION)
                    SystemBuffer)->EndOfFile;
            break;

        case FileAllocationInformation:
            if (BufferLength <
                sizeof(FILE_ALLOCATION_INFORMATION))
            {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                goto Complete;
            }
            RequestedSize =
                ((PFILE_ALLOCATION_INFORMATION)
                    SystemBuffer)->AllocationSize;
            AllocationRequest = TRUE;
            break;

        default:
            Status = STATUS_NOT_IMPLEMENTED;
            goto Complete;
    }

    if (!(FileCB->DesiredAccess & FILE_WRITE_DATA))
    {
        Status = STATUS_ACCESS_DENIED;
        goto Complete;
    }
    if (NtfsFileRecordGetHeader(FileCB->FileRec)->
            Flags & FR_IS_DIRECTORY)
    {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto Complete;
    }
    if (RequestedSize.QuadPart < 0)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Complete;
    }
    if (RequestedSize.QuadPart <
            FileCB->CommonFCBHeader.FileSize.QuadPart &&
        !MmCanFileBeTruncated(
            FileObject->SectionObjectPointer,
            &RequestedSize))
    {
        Status = STATUS_USER_MAPPED_FILE;
        goto Complete;
    }

    Status = AllocationRequest
        ? NtfsFileRecordSetFileAllocationSize(
            FileCB->FileRec,
            FileCB->RequestedType,
            FileCB->RequestedStream,
            (ULONGLONG)RequestedSize.QuadPart)
        : NtfsFileRecordSetFileDataSize(
            FileCB->FileRec,
            FileCB->RequestedType,
            FileCB->RequestedStream,
            (ULONGLONG)RequestedSize.QuadPart);
    if (NT_SUCCESS(Status))
    {
        NtfsRefreshFileSizes(FileCB,
                             FileObject);
        FileObject->Flags |=
            FO_FILE_MODIFIED |
            FO_FILE_SIZE_CHANGED;
    }

Complete:
    if (ResourceAcquired)
        ExReleaseResourceLite(
            &FileCB->MainResource);
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return Status;
}

_Function_class_(IRP_MJ_DIRECTORY_CONTROL)
_Function_class_(DRIVER_DISPATCH)
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
