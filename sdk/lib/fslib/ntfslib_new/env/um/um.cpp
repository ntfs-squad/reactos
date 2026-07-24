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
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
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
    VolumeHandle = FileHandle;
    ULONG BytesReturned;

    if (!VolumeHandle || VolumeHandle == INVALID_HANDLE_VALUE)
        return STATUS_INVALID_PARAMETER;
    
    Result = DeviceIoControl(VolumeHandle,
                             IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             NULL,
                             0,
                             &DiskGeometry,
                             sizeof(DiskGeometry),
                             &BytesReturned,
                             NULL);
    
    if (!Result)
        return STATUS_INVALID_DEVICE_REQUEST;

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

#ifndef ALIGN_UP_BY
#define ALIGN_UP_BY(size, align) (((size) + ((align) - 1)) / (align) * (align))
#endif

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    LARGE_INTEGER FileOffset;
    ULONGLONG SectorAlignedOffset;
    ULONG SectorAlignedLength;
    PUCHAR ReadBuffer;
    DWORD BytesRead;
    BOOL Result;

    if (!Length || !VolumeHandle || SectorSize == 0 || !Buffer)
        return STATUS_INVALID_PARAMETER;

    SectorAlignedOffset = Offset - (Offset % SectorSize);
    SectorAlignedLength = ALIGN_UP_BY((ULONG)((Offset - SectorAlignedOffset) + Length), SectorSize);

    if (SectorAlignedOffset == Offset && (Length % SectorSize) == 0)
    {
        FileOffset.QuadPart = Offset;
        if (!SetFilePointerEx(VolumeHandle, FileOffset, NULL, FILE_BEGIN))
            return STATUS_UNSUCCESSFUL;

        Result = ReadFile(VolumeHandle,
                          Buffer,
                          Length,
                          &BytesRead,
                          NULL);

        if (!Result || BytesRead != Length)
            return STATUS_UNSUCCESSFUL;

        return STATUS_SUCCESS;
    }

    ReadBuffer = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, SectorAlignedLength);
    if (!ReadBuffer)
        return STATUS_UNSUCCESSFUL;

    FileOffset.QuadPart = SectorAlignedOffset;
    if (!SetFilePointerEx(VolumeHandle, FileOffset, NULL, FILE_BEGIN))
    {
        HeapFree(GetProcessHeap(), 0, ReadBuffer);
        return STATUS_UNSUCCESSFUL;
    }

    Result = ReadFile(VolumeHandle,
                      ReadBuffer,
                      SectorAlignedLength,
                      &BytesRead,
                      NULL);

    if (!Result || BytesRead != SectorAlignedLength)
    {
        HeapFree(GetProcessHeap(), 0, ReadBuffer);
        return STATUS_UNSUCCESSFUL;
    }

    CopyMemory(Buffer,
               ReadBuffer + (ULONG)(Offset - SectorAlignedOffset),
               Length);

    HeapFree(GetProcessHeap(), 0, ReadBuffer);
    return STATUS_SUCCESS;
}

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer)
{
    UNREFERENCED_PARAMETER(Offset);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Buffer);

    return STATUS_NOT_IMPLEMENTED;
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
