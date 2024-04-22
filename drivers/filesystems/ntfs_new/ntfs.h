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

#define TAG_NTFS '0ftN'
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

#include "ntfsdriver.h"

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

/* Utilities */
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
