/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Usermode glue
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#define WIN32_NO_STATUS
#include <windows.h>
#include <ndk/umtypes.h> // NTSTATUS, STATUS_*, UNICODE_STRING
#include <ntfs_um.h>
#include <debug.h>

/* The core passes kernel POOL_TYPE values into the allocation contract;
 * the heap-backed usermode allocator ignores them, so the underlying
 * integer type is all we need here.
 */
typedef int POOL_TYPE;

typedef BOOLEAN (NTAPI *PRtlIsNameInExpression)(
    _In_     PUNICODE_STRING Expression,
    _In_     PUNICODE_STRING Name,
    _In_     BOOLEAN         IgnoreCase,
    _In_opt_ PWCH            UpcaseTable
);

BOOLEAN NtfsIsNameInExpressionFallback(
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

    // Check if we have RtlIsNameInExpression() in ntdll. If we do, save the pointer.
    pRtlIsNameInExpression = reinterpret_cast<PRtlIsNameInExpression>(GetProcAddress(GetModuleHandleA("ntdll.dll"),
                                                                                     "RtlIsNameInExpression"));

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

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    // Hack: We're assuming buffering is used in readfile below.
    if (!ReadFile(VolumeHandle,
                  Buffer,
                  Length,
                  NULL,
                  NULL))
        return STATUS_UNSUCCESSFUL;
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
    // If we have RtlIsNameInExpression() in ntdll, use that.
    if (pRtlIsNameInExpression)
    {
        return pRtlIsNameInExpression(Expression,
                                      Name,
                                      IgnoreCase,
                                      UpcaseTable);
    }

    // If we don't, use the simpler and portable fallback implementation.
    return NtfsIsNameInExpressionFallback(Expression,
                                          Name,
                                          IgnoreCase,
                                          UpcaseTable);
}

#ifdef __cplusplus
}
#endif
