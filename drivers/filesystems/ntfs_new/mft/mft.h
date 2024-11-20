/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

typedef class MasterFileTable
{
public:
    UINT FileRecordSize;

    MasterFileTable(_In_ PNTFSVolume TargetVolume,
                    _In_ UINT64 MFTLCN,
                    _In_ UINT64 MFTMirrLCN,
                    _In_ INT8   ClustersPerFileRecord);
    // ~MasterFileTable();

    NTSTATUS
    GetFileRecord(_In_   ULONG FileRecordNumber,
                  _Out_  PFileRecord* File);

    NTSTATUS
    GetFileRecordFromMFTMirr(_In_  ULONG FileRecordNumber,
                             _Out_ PFileRecord* File);

    NTSTATUS
    GetFileRecordFromQuery(_In_ PWCHAR Query,
                           _Out_ PFileRecord* File);

    NTSTATUS
    WriteFileRecordToMFT(_In_ PFileRecord File);

    NTSTATUS
    IsFileRecordNumberInUse(_In_  ULONG FileRecordNumber,
                            _Out_ PBOOLEAN InUse);

private:
    PNTFSVolume Volume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    INT    MftZoneReservation;
    PFileRecord MFTFile = NULL;
    PFileRecord MFTMirrFile = NULL;
} *PMasterFileTable;