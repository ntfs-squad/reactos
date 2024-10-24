/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

class MFT
{
public:
    MFT(_In_ PNTFSVolume TargetVolume,
        _In_ UINT64 MFTLCN,
        _In_ UINT64 MFTMirrLCN,
        _In_ INT8   ClustersPerFileRecord);
    // ~MFT();

    NTSTATUS
    GetFileRecord(_In_   ULONGLONG FileRecordNumber,
                  _Out_  PFileRecord* File);

    // NTSTATUS
    // WriteFileRecord(_In_ ULONGLONG FileRecordNumber,
    //                 _In_ PULONG BufferLength,
    //                 _In_ PUCHAR Buffer);
private:
    PNTFSVolume Volume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    UINT   FileRecordSize;
};