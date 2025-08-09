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
    USHORT FileNameLength;

    if (VolumeDeviceObject == NtfsDiskFileSystemDeviceObject)
    {
        /* DeviceObject represents FileSystem instead of logical volume */
        DPRINT1("Opening file system\n");
        Irp->IoStatus.Information = FILE_OPENED;
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
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
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
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
                /* Create a new file record, assign a free FRN, persist to $MFT,
                 * and add an entry to the parent directory (root for now).
                 */
                {
                    // Allocate a new empty file record buffer
                    CurrentFile = new(PagedPool, 'rftN') FileRecord(Volume);
                    RtlZeroMemory(CurrentFile->Data, Volume->MFT->FileRecordSize);

                    // Initialize FILE record header
                    CurrentFile->Header->Header.TypeID[0] = 'F';
                    CurrentFile->Header->Header.TypeID[1] = 'I';
                    CurrentFile->Header->Header.TypeID[2] = 'L';
                    CurrentFile->Header->Header.TypeID[3] = 'E';
                    CurrentFile->Header->SequenceNumber = 1;
                    CurrentFile->Header->HardLinkCount = 1;
                    // Set flags/directory structures if creating a directory
                    if (IrpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE)
                    {
                        CurrentFile->Header->Flags = FR_IS_DIRECTORY;
                    }
                    else
                    {
                        CurrentFile->Header->Flags = 0; // regular file
                    }
                    CurrentFile->Header->AllocatedSize = Volume->MFT->FileRecordSize;
                    CurrentFile->Header->BaseFileRecord = 0;
                    CurrentFile->Header->NextAttributeID = 1;
                    CurrentFile->Header->MFTRecordNumber = 0xFFFFFFFF; // sentinel: not on disk

                    // Set up Update Sequence Array (USA) and AttributeOffset
                    {
                        USHORT sectorsPerRecord = (USHORT)(Volume->MFT->FileRecordSize / Volume->BytesPerSector);
                        USHORT usaOffset = sizeof(FileRecordHeader);
                        USHORT usaTotalBytes = (USHORT)(sizeof(USHORT) /*USN*/ + sectorsPerRecord * sizeof(USHORT));
                        CurrentFile->Header->Header.UpdateSequenceOffset = usaOffset;
                        CurrentFile->Header->Header.SizeOfUpdateSequence = (USHORT)(sectorsPerRecord + 1);
                        CurrentFile->Header->AttributeOffset = (USHORT)ROUND_UP(usaOffset + usaTotalBytes, 8);
                        // Zero the USA area
                        RtlZeroMemory(CurrentFile->Data + usaOffset, usaTotalBytes);
                    }

                    // Write $STANDARD_INFORMATION resident attribute
                    PUCHAR attrPtr = CurrentFile->Data + CurrentFile->Header->AttributeOffset;
                    PAttribute StdAttr = (PAttribute)attrPtr;
                    RtlZeroMemory(StdAttr, sizeof(Attribute));
                    StdAttr->AttributeType = TypeStandardInformation;
                    StdAttr->IsNonResident = 0;
                    StdAttr->NameLength = 0;
                    StdAttr->NameOffset = 0;
                    StdAttr->Resident.DataOffset = 0x18;
                    StdAttr->Resident.DataLength = sizeof(StandardInformationEx);
                    ULONG StdAttrLen = ROUND_UP(StdAttr->Resident.DataOffset + StdAttr->Resident.DataLength, 8);
                    StdAttr->Length = StdAttrLen;

                    PStandardInformationEx StdInfo = (PStandardInformationEx) GetResidentDataPointer(StdAttr);
                    RtlZeroMemory(StdInfo, sizeof(StandardInformationEx));
                    StdInfo->FilePermissions = FILE_PERM_ARCHIVE | FILE_PERM_NORMAL;

                    // Advance pointer
                    attrPtr += StdAttr->Length;

                    // Write empty $DATA resident attribute
                    PAttribute DataAttr = (PAttribute)attrPtr;
                    RtlZeroMemory(DataAttr, sizeof(Attribute));
                    DataAttr->AttributeType = TypeData;
                    DataAttr->IsNonResident = 0;
                    DataAttr->NameLength = 0;
                    DataAttr->NameOffset = 0;
                    DataAttr->Resident.DataOffset = 0x18;
                    DataAttr->Resident.DataLength = 0;
                    ULONG DataAttrLen = ROUND_UP(DataAttr->Resident.DataOffset + DataAttr->Resident.DataLength, 8);
                    if (DataAttrLen == 0) DataAttrLen = 0x18; // minimal resident header size
                    DataAttr->Length = DataAttrLen;

                    attrPtr += DataAttr->Length;

                    // End marker
                    PAttribute EndAttr = (PAttribute)attrPtr;
                    RtlZeroMemory(EndAttr, sizeof(Attribute));
                    EndAttr->AttributeType = TypeAttributeEndMarker;
                    EndAttr->Length = 0;

                    // If directory, append a minimal $INDEX_ROOT ("$I30")
                    if (CurrentFile->Header->Flags & FR_IS_DIRECTORY)
                    {
                        PAttribute IdxRoot = (PAttribute)attrPtr;
                        RtlZeroMemory(IdxRoot, sizeof(Attribute));
                        IdxRoot->AttributeType = TypeIndexRoot;
                        IdxRoot->IsNonResident = 0;
                        IdxRoot->NameLength = 4; // "$I30"
                        IdxRoot->NameOffset = 0x18;
                        // Data starts after name
                        IdxRoot->Resident.DataOffset = 0x18 + (4 * sizeof(WCHAR));
                        // Build IndexRootEx with a single END entry
                        PIndexRootEx ir = (PIndexRootEx)((PUCHAR)IdxRoot + IdxRoot->Resident.DataOffset);
                        RtlZeroMemory(ir, sizeof(IndexRootEx));
                        ir->AttributeType = TypeFileName;
                        ir->CollationRule = ATTRDEF_COLLATION_FILENAME;
                        ir->BytesPerIndexRec = BytesPerCluster(Volume);
                        ir->ClusPerIndexRec = 1;
                        ir->Header.IndexOffset = sizeof(IndexNodeHeader);
                        USHORT endLen = (USHORT)ROUND_UP(sizeof(IndexEntry), 8);
                        ir->Header.TotalIndexSize = (UINT16)(sizeof(IndexNodeHeader) + endLen);
                        ir->Header.AllocatedSize = (UINT16)ROUND_UP(ir->Header.TotalIndexSize, 8);
                        // Write END entry
                        PIndexEntry endEntry = (PIndexEntry)(((PUCHAR)&ir->Header) + ir->Header.IndexOffset);
                        RtlZeroMemory(endEntry, endLen);
                        endEntry->EntryLength = endLen;
                        endEntry->StreamLength = 0;
                        endEntry->Flags = INDEX_ENTRY_END;
                        // DataLength must include the IndexRootEx preamble and the index node payload
                        IdxRoot->Resident.DataLength = (ULONG)(FIELD_OFFSET(IndexRootEx, Header) + ir->Header.TotalIndexSize);
                        // Attribute length rounded
                        ULONG idxLen = ROUND_UP(IdxRoot->Resident.DataOffset + IdxRoot->Resident.DataLength, 8);
                        IdxRoot->Length = idxLen;
                        // Name
                        PWCHAR nm = (PWCHAR)((PUCHAR)IdxRoot + 0x18);
                        nm[0] = L'$'; nm[1] = L'I'; nm[2] = L'3'; nm[3] = L'0';
                        attrPtr += IdxRoot->Length;
                    }

                    // Append attribute end marker
                    {
                        PAttribute AttrEnd = (PAttribute)attrPtr;
                        RtlZeroMemory(AttrEnd, sizeof(Attribute));
                        AttrEnd->AttributeType = TypeAttributeEndMarker;
                        AttrEnd->Length = 0;
                        attrPtr += sizeof(Attribute);
                    }
                    // Finalize record size
                    CurrentFile->Header->ActualSize = (ULONG)((ULONG_PTR)attrPtr - (ULONG_PTR)CurrentFile->Data);

                    // Allocate a file record number
                    ULONG NewFrn;
                    Status = Volume->MFT->AllocateFreeFileRecord(&NewFrn);
                    if (!NT_SUCCESS(Status))
                    {
                        delete CurrentFile;
                        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                        Irp->IoStatus.Status = Status;
                        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                        return Status;
                    }
                    CurrentFile->Header->MFTRecordNumber = NewFrn;

                    // Persist the file record to $MFT
                    Status = Volume->MFT->WriteFileRecordToMFT(CurrentFile);
                    if (!NT_SUCCESS(Status))
                    {
                        delete CurrentFile;
                        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                        Irp->IoStatus.Status = Status;
                        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                        return Status;
                    }

                    // Add a directory entry in the parent directory (root for now)
                    PFileRecord RootFile;
                    if (!NT_SUCCESS(Volume->MFT->GetFileRecord(_Root, &RootFile)))
                    {
                        delete CurrentFile;
                        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                        Irp->IoStatus.Status = STATUS_SUCCESS; // allow creation even if directory entry cannot be created yet
                        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                        return STATUS_SUCCESS;
                    }
                    Directory ParentDir(Volume);
                    Status = ParentDir.LoadDirectory(RootFile);
                    if (!NT_SUCCESS(Status))
                    {
                        delete RootFile;
                        delete CurrentFile;
                        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                        Irp->IoStatus.Status = Status;
                        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                        return Status;
                    }

                    // Build a FILE_NAME stream for the new entry from requested name
                    USHORT nameChars = 0;
                    PWCHAR nameBuf = FileObject->FileName.Buffer;
                    if (nameBuf)
                    {
                        PWCHAR last = wcsrchr(nameBuf, L'\\');
                        PWCHAR comp = last ? last + 1 : nameBuf;
                        nameChars = (USHORT)wcslen(comp);
                        SIZE_T alloc = sizeof(FileNameEx) - sizeof(WCHAR) + nameChars * sizeof(WCHAR);
                        PFileNameEx fn = (PFileNameEx)ExAllocatePoolWithTag(PagedPool, alloc, TAG_FILE_RECORD);
                        if (!fn)
                        {
                            delete RootFile;
                            delete CurrentFile;
                            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                            return STATUS_INSUFFICIENT_RESOURCES;
                        }
                        RtlZeroMemory(fn, (ULONG)alloc);
                        fn->NameLength = nameChars;
                        fn->NameType = NAME_TYPE_WIN32;
                        RtlCopyMemory(fn->Name, comp, nameChars * sizeof(WCHAR));

                        // Append resident $FILE_NAME attribute to file record just before END marker
                        {
                            // Scan to end marker
                            ULONG off = CurrentFile->Header->AttributeOffset;
                            PAttribute cur = (PAttribute)(CurrentFile->Data + off);
                            while (cur->AttributeType != TypeAttributeEndMarker) {
                                if (cur->Length == 0) { ExFreePoolWithTag(fn, TAG_FILE_RECORD); delete RootFile; delete CurrentFile; Irp->IoStatus.Information = FILE_DOES_NOT_EXIST; Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR; IoCompleteRequest(Irp, IO_DISK_INCREMENT); return STATUS_FILE_CORRUPT_ERROR; }
                                off += cur->Length;
                                cur = (PAttribute)(CurrentFile->Data + off);
                            }
                            // cur points to END marker. Write FILE_NAME attribute here
                            PAttribute FileNameAttr = cur;
                            RtlZeroMemory(FileNameAttr, sizeof(Attribute));
                            FileNameAttr->AttributeType = TypeFileName;
                            FileNameAttr->IsNonResident = 0;
                            FileNameAttr->NameLength = 0;
                            FileNameAttr->NameOffset = 0;
                            FileNameAttr->Resident.DataOffset = 0x18;
                            FileNameAttr->Resident.DataLength = (ULONG)alloc;
                            ULONG fnAttrLen = ROUND_UP(FileNameAttr->Resident.DataOffset + FileNameAttr->Resident.DataLength, 8);
                            FileNameAttr->Length = fnAttrLen;
                            // Payload
                            PFileNameEx fnPayload = (PFileNameEx)((PUCHAR)FileNameAttr + FileNameAttr->Resident.DataOffset);
                            RtlCopyMemory(fnPayload, fn, (ULONG)alloc);
                            fnPayload->ParentFileReference = _Root; // root for now
                            // Advance pointer and write new END marker
                            PAttribute NewEnd = (PAttribute)((PUCHAR)FileNameAttr + fnAttrLen);
                            RtlZeroMemory(NewEnd, sizeof(Attribute));
                            NewEnd->AttributeType = TypeAttributeEndMarker;
                            NewEnd->Length = 0;
                            // Update record size
                            CurrentFile->Header->ActualSize += (fnAttrLen + sizeof(Attribute));
                        }

                        // Persist the file again to include the $FILE_NAME attribute
                        Status = Volume->MFT->WriteFileRecordToMFT(CurrentFile);
                        if (!NT_SUCCESS(Status))
                        {
                            ExFreePoolWithTag(fn, TAG_FILE_RECORD);
                            delete RootFile;
                            delete CurrentFile;
                            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                            Irp->IoStatus.Status = Status;
                            IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                            return Status;
                        }

                        Status = ParentDir.AddFileToDirectory(fn, NewFrn);
                        ExFreePoolWithTag(fn, TAG_FILE_RECORD);
                        if (!NT_SUCCESS(Status))
                        {
                            // If directory index update not supported yet (e.g., nonresident),
                            // allow creation to succeed without index visibility.
                            if (Status != STATUS_NOT_IMPLEMENTED && Status != STATUS_FILE_CORRUPT_ERROR)
                            {
                                delete RootFile;
                                delete CurrentFile;
                                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                                Irp->IoStatus.Status = Status;
                                IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                                return Status;
                            }
                        }
                    }

                    delete RootFile;
                    Status = STATUS_SUCCESS;
                }
                break;
                break;
            case FILE_OPEN:
            case FILE_OVERWRITE:
            default:
                // In these cases, return an error.
                Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                IoCompleteRequest(Irp, IO_DISK_INCREMENT);
                return STATUS_INVALID_PARAMETER;
                break;
        }

    }

    // Create file context block.
    FileCB = new(NonPagedPool) FileContextBlock();
    RtlZeroMemory(FileCB, sizeof(FileContextBlock));
    
    // Initialize the NT required FCB header and resources
    FsRtlInitializeFileLock(&FileCB->FileLock, NULL, NULL);
    ExInitializeResourceLite(&FileCB->MainResource);
    ExInitializeResourceLite(&FileCB->PagingIoResource);
    ExInitializeFastMutex(&FileCB->HeaderMutex);
    FsRtlSetupAdvancedHeader(&FileCB->CommonFCBHeader, &FileCB->HeaderMutex);
    FileCB->CommonFCBHeader.Resource = &FileCB->MainResource;
    FileCB->CommonFCBHeader.PagingIoResource = &FileCB->PagingIoResource;
    FileCB->CommonFCBHeader.IsFastIoPossible = FastIoIsPossible;

    // Set file name
    FileNameLength = IrpSp->FileObject->FileName.Length;
    PWCHAR FileNameBuffer = new(PagedPool) WCHAR[FileNameLength];
    RtlCopyMemory(FileNameBuffer,
                  IrpSp->FileObject->FileName.Buffer,
                  FileNameLength);
    FileCB->FileName.Buffer = FileNameBuffer;
    FileCB->FileName.Length = FileNameLength;
    FileCB->FileName.MaximumLength = FileNameLength;
    // NOTE: FileNameBuffer gets freed when the FileCB is cleaned up.

    // Get ADS Preferences for the file.
    Status = Volume->GetADSPreference(FileObject,
                                      &FileCB->RequestedType,
                                      &FileCB->RequestedStream);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to get ADS preference! Aborting...\n");
        delete FileCB;
        Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_DISK_INCREMENT);
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

    // Initialize cache map on first open when we have valid sizes
    {
        // Use the CommonFCBHeader fields as the canonical CC_FILE_SIZES storage
        PCC_FILE_SIZES FileSizes = (PCC_FILE_SIZES)&FileCB->CommonFCBHeader.AllocationSize;
        // Initialize the common header sizes from attributes
        PAttribute DataAttr = CurrentFile->GetAttribute(TypeData, NULL);
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

        // Set SectionObjectPointers on FILE_OBJECT and initialize cache map
        FileObject->SectionObjectPointer = &FileCB->SectionObjectPointers;
        RtlZeroMemory(&FileCB->SectionObjectPointers, sizeof(SECTION_OBJECT_POINTERS));

        // Build callbacks for Cache Manager
        CACHE_MANAGER_CALLBACKS Callbacks = {0};
        Callbacks.AcquireForLazyWrite  = NtfsAcqLazyWrite;
        Callbacks.ReleaseFromLazyWrite = NtfsRelLazyWrite;
        Callbacks.AcquireForReadAhead  = NtfsAcqReadAhead;
        Callbacks.ReleaseFromReadAhead = NtfsRelReadAhead;

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

    // Set FsContext to the file context block and open file.
    FileObject->FsContext = FileCB;
    Irp->IoStatus.Information = (!NT_SUCCESS(Status)) ? 0 :
        ((Disposition == FILE_CREATE || Disposition == FILE_OPEN_IF || Disposition == FILE_OVERWRITE_IF || Disposition == FILE_SUPERSEDE) ? FILE_CREATED : FILE_OPENED);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_DISK_INCREMENT);
    return STATUS_SUCCESS;
}
