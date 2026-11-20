/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Usermode glue
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include <windows.h>
#include <subauth.h>
#include <ntfs_um.h>
#include <debug.h>

// Hack: we shouldn't be defining these status codes.
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                   1
#endif
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)

// Hack: We shouldn't be using these pools in UM at all.
typedef enum _POOL_TYPE
{
    NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed,
    DontUseThisType,
    NonPagedPoolCacheAligned,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS,
    MaxPoolType,

    NonPagedPoolBase = 0,
    NonPagedPoolBaseMustSucceed = NonPagedPoolBase + 2,
    NonPagedPoolBaseCacheAligned = NonPagedPoolBase + 4,
    NonPagedPoolBaseCacheAlignedMustS = NonPagedPoolBase + 6,

    NonPagedPoolSession = 32,
    PagedPoolSession,
    NonPagedPoolMustSucceedSession,
    DontUseThisTypeSession,
    NonPagedPoolCacheAlignedSession,
    PagedPoolCacheAlignedSession,
    NonPagedPoolCacheAlignedMustSSession
} POOL_TYPE;

typedef BOOLEAN (NTAPI *PRtlIsNameInExpression)(
    _In_     PUNICODE_STRING Expression,
    _In_     PUNICODE_STRING Name,
    _In_     BOOLEAN         IgnoreCase,
    _In_opt_ PWCH            UpcaseTable
);

HANDLE VolumeHandle = NULL;
ULONG SectorSize = 0;
static PRtlIsNameInExpression pRtlIsNameInExpression;

NTSTATUS
NtfsDiskInitializeUm(
    _In_      HANDLE FileHandle,
    _Out_opt_ ULONG* BytesPerSector)
{
    BOOL Result;
    DISK_GEOMETRY DiskGeometry;
    ULONG BytesReturned;

    if (!FileHandle || FileHandle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_PARAMETER;
    
    Result = DeviceIoControl(FileHandle,
                             IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             NULL,
                             0,
                             &DiskGeometry,
                             sizeof(DiskGeometry),
                             &BytesReturned,
                             NULL);
    
    if (!Result)
        return STATUS_INVALID_DEVICE_REQUEST;

    VolumeHandle = FileHandle;
    SectorSize = DiskGeometry.BytesPerSector;

    if (BytesPerSector)
        *BytesPerSector = SectorSize;

    pRtlIsNameInExpression = reinterpret_cast<PRtlIsNameInExpression>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"),
                       "RtlIsNameInExpression"));

    if(!pRtlIsNameInExpression)
    {
        pRtlIsNameInExpression = reinterpret_cast<PRtlIsNameInExpression>(
            GetProcAddress(GetModuleHandleA("ntdll_vista.dll"),
                           "RtlIsNameInExpression"));
    }

    return STATUS_SUCCESS;
}

#ifdef __cplusplus
extern "C" {
#endif

void*
NtfsAllocatePoolWithTag(POOL_TYPE PoolType, size_t Size, ULONG Tag)
{
    UNREFERENCED_PARAMETER(PoolType);
    UNREFERENCED_PARAMETER(Tag);
    return HeapAlloc(GetProcessHeap(), 0, Size);
}

void
NtfsFreePool(void* pObject)
{
    HeapFree(GetProcessHeap(), 0, pObject);
}

void
NtfsFillMemory(_In_ PVOID Buffer,
               _In_ size_t Size,
               _In_ UCHAR Value)
{
    FillMemory(Buffer, Size, Value);
}

ULONG
NtfsGetBytesPerSector(VOID)
{
    return SectorSize;
}

// Use explicit offsets on the synchronous handle. Callers supply any required alignment.
NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    OVERLAPPED Position = {};
    DWORD BytesRead;

    if (!VolumeHandle || !Buffer || !Length || Offset > MAXLONGLONG)
        return STATUS_INVALID_PARAMETER;

    Position.Offset = (DWORD)Offset;
    Position.OffsetHigh = (DWORD)(Offset >> 32);
    if (!ReadFile(VolumeHandle, Buffer, Length, &BytesRead, &Position) || BytesRead != Length)
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer)
{
    OVERLAPPED Position = {};
    DWORD BytesWritten;

    if (!VolumeHandle || !Buffer || !Length || Offset > MAXLONGLONG)
        return STATUS_INVALID_PARAMETER;

    Position.Offset = (DWORD)Offset;
    Position.OffsetHigh = (DWORD)(Offset >> 32);
    if (!WriteFile(VolumeHandle, Buffer, Length, &BytesWritten, &Position) || BytesWritten != Length)
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

BOOLEAN
NtfsIsNameInExpression(_In_     PUNICODE_STRING Expression,
                       _In_     PUNICODE_STRING Name,
                       _In_     BOOLEAN IgnoreCase,
                       _In_opt_ PWCHAR UpcaseTable)
{
    ASSERT(pRtlIsNameInExpression);
    return pRtlIsNameInExpression(Expression,
                                  Name,
                                  IgnoreCase,
                                  UpcaseTable);
}

#ifdef __cplusplus
}
#endif
