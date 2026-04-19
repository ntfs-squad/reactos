/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

 #define IsFileRecordInMFTMirr(FileRecordNumber) \
 ((Volume->SectorsPerCluster * Volume->BytesPerSector) > (FileRecordSize << 2)) ? \
 ((Volume->SectorsPerCluster * Volume->BytesPerSector) / FileRecordSize) < FileRecordNumber \
 : FileRecordNumber < 4
 
 typedef class MasterFileTable
 {
 public:
     UINT FileRecordSize;
 
     // ./ mft.cpp
     MasterFileTable(_In_ PNTFSVolume TargetVolume,
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
     PNTFSVolume Volume;
     UINT64 MFTLCN;
     UINT64 MFTMirrLCN;
     INT    MftZoneReservation;
     PFileRecord MFTFile = NULL;
     PFileRecord MFTMirrFile = NULL;
 } *PMasterFileTable;
 