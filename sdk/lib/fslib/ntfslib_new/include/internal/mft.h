/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#define IsFileRecordInMFTMirr(FileRecordNumber) \
((DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector) > (FileRecordSize << 2)) ? \
((DiskVolume->SectorsPerCluster * DiskVolume->BytesPerSector) / FileRecordSize) < FileRecordNumber \
: FileRecordNumber < 4

#ifdef __cplusplus

typedef class MasterFileTable
{
public:
    UINT FileRecordSize;

    // ./ mft.cpp
    MasterFileTable(_In_ PVolume TargetVolume,
                    _In_ UINT64 MFTLCN,
                    _In_ UINT64 MFTMirrLCN,
                    _In_ INT8   ClustersPerFileRecord);

    NTSTATUS
    WriteFileRecordToMFT(_In_ PFileRecord File);

    NTSTATUS
    IsFileRecordNumberInUse(_In_  ULONG FileRecordNumber,
                            _Out_ PBOOLEAN InUse);

    // ./get.cpp
    NTSTATUS
    GetFileRecord(_In_   ULONG FileRecordNumber,
                  _Out_  PFileRecord* File);

    NTSTATUS
    GetFileRecordFromMFTMirr(_In_  ULONG FileRecordNumber,
                             _Out_ PFileRecord* File);

    NTSTATUS
    GetFileRecordFromQuery(_In_  PWCHAR Query,
                           _Out_ PFileRecord* File);

    NTSTATUS
    GetFileAttributeFromFileRecordNumber(_In_  AttributeType Type,
                                         _In_  PWSTR Name,
                                         _In_  ULONG FileRecordNumber,
                                         _Out_ PFileRecord* TargetFile,
                                         _Out_ PAttribute* TargetAttribute);

private:
    PVolume DiskVolume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    INT    MftZoneReservation;
    PFileRecord MFTFile = NULL;
    PFileRecord MFTMirrFile = NULL;
} *PMasterFileTable;

#endif // __cplusplus
