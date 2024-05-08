/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Source file for the ntfs_new entry point
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl J. Bialorucki <carl.bialorucki@reactos.org>
 */

#pragma once

#include <ntifs.h>
#include <pseh/pseh2.h>
#include <section_attribs.h>

#define TAG_NTFS 'NTFS'
#define TAG_CCB 'CftN'
#define TAG_FCB 'FftN'
#define TAG_IRP_CTXT 'iftN'
#define TAG_ATT_CTXT 'aftN'
#define TAG_FILE_REC 'rftN'

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))

#define DEVICE_NAME L"\\Ntfs"
#define IRPCONTEXT_CANWAIT 0x1
#define IRPCONTEXT_COMPLETE 0x2
#define IRPCONTEXT_QUEUE 0x4

typedef struct
{

    ULONG Flags;
    PIO_STACK_LOCATION Stack;
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    PIRP Irp;
    BOOLEAN IsTopLevel;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject;
    CCHAR PriorityBoost;
} NTFS_IRP_CONTEXT, *PNTFS_IRP_CONTEXT;

/* fastio */
BOOLEAN NTAPI
NtfsAcqLazyWrite(PVOID Context,
                 BOOLEAN Wait);

VOID NTAPI
NtfsRelLazyWrite(PVOID Context);

BOOLEAN NTAPI
NtfsAcqReadAhead(PVOID Context,
                 BOOLEAN Wait);

VOID NTAPI
NtfsRelReadAhead(PVOID Context);

FAST_IO_CHECK_IF_POSSIBLE NtfsFastIoCheckIfPossible;
FAST_IO_READ NtfsFastIoRead;
FAST_IO_WRITE NtfsFastIoWrite;


#include <pshpack1.h>
typedef struct _BIOS_PARAMETERS_BLOCK
{
    USHORT    BytesPerSector;			// 0x0B
    UCHAR     SectorsPerCluster;		// 0x0D
    UCHAR     Unused0[7];				// 0x0E, checked when volume is mounted
    UCHAR     MediaId;				// 0x15
    UCHAR     Unused1[2];				// 0x16
    USHORT    SectorsPerTrack;		// 0x18
    USHORT    Heads;					// 0x1A
    UCHAR     Unused2[4];				// 0x1C
    UCHAR     Unused3[4];				// 0x20, checked when volume is mounted
} BIOS_PARAMETERS_BLOCK, *PBIOS_PARAMETERS_BLOCK;

typedef struct _EXTENDED_BIOS_PARAMETERS_BLOCK
{
    USHORT    Unknown[2];				// 0x24, always 80 00 80 00
    ULONGLONG SectorCount;			// 0x28
    ULONGLONG MftLocation;			// 0x30
    ULONGLONG MftMirrLocation;		// 0x38
    CHAR      ClustersPerMftRecord;	// 0x40
    UCHAR     Unused4[3];				// 0x41
    CHAR      ClustersPerIndexRecord; // 0x44
    UCHAR     Unused5[3];				// 0x45
    ULONGLONG SerialNumber;			// 0x48
    UCHAR     Checksum[4];			// 0x50
} EXTENDED_BIOS_PARAMETERS_BLOCK, *PEXTENDED_BIOS_PARAMETERS_BLOCK;

typedef struct _BOOT_SECTOR
{
    UCHAR     Jump[3];				// 0x00
    UCHAR     OEMID[8];				// 0x03
    BIOS_PARAMETERS_BLOCK BPB;
    EXTENDED_BIOS_PARAMETERS_BLOCK EBPB;
    UCHAR     BootStrap[426];			// 0x54
    USHORT    EndSector;				// 0x1FE
} BOOT_SECTOR, *PBOOT_SECTOR;
#include <poppack.h>

/* Utilities */
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);


#include "ntfsdriver.h"
#include "ntfspartition.h"

NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceBeingRead,
         _In_    LONGLONG StartingOffset,
         _In_    ULONG AmountOfSectors,
         _In_    ULONG SectorSize,
         _Inout_ PUCHAR Buffer,
         _In_    BOOLEAN Override);

NTSTATUS
ReadBlock(_In_    PDEVICE_OBJECT DeviceObject,
          _In_    ULONG DiskSector,
          _In_    ULONG SectorCount,
          _In_    ULONG SectorSize,
          _Inout_ PUCHAR Buffer,
          _In_    BOOLEAN Override);

NTSTATUS
DeviceIoControl(_In_    PDEVICE_OBJECT DeviceObject,
                           _In_    ULONG ControlCode,
                           _In_    PVOID InputBuffer,
                           _In_    ULONG InputBufferSize,
                           _Inout_ PVOID OutputBuffer,
                           _Inout_ PULONG OutputBufferSize,
                           _In_    BOOLEAN Override);