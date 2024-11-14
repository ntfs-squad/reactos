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
    GetFileRecordFromQuery(_In_ PWCHAR Query,
                           _Out_ PFileRecord* File);

    NTSTATUS
    GetFileRecordDiskOffset(_In_ ULONG FileRecordNumber,
                            _Out_ PULONGLONG FileRecordDiskOffset);

private:
    PNTFSVolume Volume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    INT    MftZoneReservation;
} *PMasterFileTable;