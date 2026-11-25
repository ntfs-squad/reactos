/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Usermode glue
 * COPYRIGHT:   Copyright 2026 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#define WIN32_NO_STATUS
#include <windows.h>
#include <ndk/umtypes.h>
#include <ndk/mmtypes.h>
#include <ntfs_um.h>
#include <debug.h>
#include "../../include/internal/ntfslib_new_internal_alloc.h"

/* In theory, as documented, we can call ReadFile() and WriteFile() with any
 * length. In practice, if the length is larger than what can be represented
 * in one MDL, it fails -- even on Windows. The maximum size that can be
 * represented in one MDL varies by NT version and architecture.
 *
 * The calculation for Windows XP/Windows Server 2003/NT5.2 and older is:
 *     PAGE_SIZE * (MaxMDLPayloadSize / sizeof(PFN_NUMBER)), where:
 *         - MaxMDLPayloadSize = MAXUSHORT - sizeof(MDL)
 *     Note: MAXUSHORT is 65535, and PFN_NUMBER is a typedef of ULONG_PTR.
 *
 * The calculation for Windows Vista/Server 2008/NT6.0 is:
 *     2 GiB - PAGE_SIZE
 *
 * The calculation for Windows 7/Server 2008 R2/NT 6.1 and newer is:
 *     4 GiB - PAGE_SIZE
 *
 * See also: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioallocatemdl#remarks
 *           https://mskb.pkisolutions.com/kb/2484430
 */
#if NTDDI_VERSION <= NTDDI_WS03SP4 || defined(__REACTOS__) // TODO: Get rid of this pin when ReactOS can support larger MDLs.
#define MDL_SIZE \
    ALIGN_UP(4 * sizeof(PVOID) + 2 * sizeof(CSHORT) + 2 * sizeof(ULONG), PVOID)
#define MDL_MAX_PAYLOAD_SIZE (MAXUSHORT - MDL_SIZE)
#define MAX_BYTES_PER_MDL ((ULONG)(PAGE_SIZE * (MDL_MAX_PAYLOAD_SIZE / sizeof(ULONG_PTR))))
#elif NTDDI_VERSION <= NTDDI_VISTASP4
#define MAX_BYTES_PER_MDL ((ULONG)((1ULL << 31) - PAGE_SIZE))
#else
#define MAX_BYTES_PER_MDL ((ULONG)((1ULL << 32) - PAGE_SIZE))
#endif

typedef BOOLEAN (NTAPI *PRtlIsNameInExpression)(
    _In_     PUNICODE_STRING Expression,
    _In_     PUNICODE_STRING Name,
    _In_     BOOLEAN         IgnoreCase,
    _In_opt_ PWCH            UpcaseTable
);

HANDLE VolumeHandle = NULL;
ULONG SectorSize = 0;
static PRtlIsNameInExpression pRtlIsNameInExpression;

static BOOLEAN NTAPI
NtfsIsNameInExpressionStub(_In_     PUNICODE_STRING Expression,
                           _In_     PUNICODE_STRING Name,
                           _In_     BOOLEAN         IgnoreCase,
                           _In_opt_ PWCH            UpcaseTable)
{
    UNREFERENCED_PARAMETER(Expression);
    UNREFERENCED_PARAMETER(Name);
    UNREFERENCED_PARAMETER(IgnoreCase);
    UNREFERENCED_PARAMETER(UpcaseTable);

    /* This is just enough to get Ntfsinfo to work on Windows Sever 2003.
     * Ntfsinfo doesn't need to use this function, since it queries files by FRN.
     */
    return FALSE;
}

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

    if (!pRtlIsNameInExpression)
    {
        // ReactOS has an RtlIsNameInExpression implementation in ntdll_vista.
        HMODULE NtDllVista = GetModuleHandleA("ntdll_vista.dll");
        if (NtDllVista)
        {
            pRtlIsNameInExpression = reinterpret_cast<PRtlIsNameInExpression>(
                GetProcAddress(NtDllVista, "RtlIsNameInExpression"));
        }
    }

    /* Windows Vista/NT 6.0 and older don't have an RtlIsNameInExpression
     * implementation. Use a stub.
     */
    if (!pRtlIsNameInExpression)
        pRtlIsNameInExpression = NtfsIsNameInExpressionStub;

    ASSERT(pRtlIsNameInExpression);
    return STATUS_SUCCESS;
}

#ifdef __cplusplus
extern "C" {
#endif

void*
NtfsAllocatePoolWithTag(NTFS_POOL PoolType, size_t Size, ULONG Tag)
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

VOID
NtfsAssertPagedCode(VOID)
{
}

ULONG
NtfsGetBytesPerSector(VOID)
{
    return SectorSize;
}

static NTSTATUS
AccessVolumeAligned(_In_    ULONGLONG Offset,
                    _In_    ULONG Length,
                    _Inout_ PUCHAR Buffer,
                    _In_    BOOLEAN Write)
{
    BOOL Result;
    DWORD BytesAccessed;

    // Assert that the buffer is page aligned
    ASSERT(reinterpret_cast<ULONG_PTR>(Buffer) % PAGE_SIZE == 0);

    while (Length)
    {
        OVERLAPPED Position = {};
        ULONG ChunkLength = min(Length, MAX_BYTES_PER_MDL);
        Position.Offset = (DWORD)Offset;
        Position.OffsetHigh = (DWORD)(Offset >> 32);

        if (Write)
            Result = WriteFile(VolumeHandle, Buffer, ChunkLength, &BytesAccessed, &Position);
        else
            Result = ReadFile(VolumeHandle, Buffer, ChunkLength, &BytesAccessed, &Position);

        if (!Result || BytesAccessed != ChunkLength)
            return STATUS_UNSUCCESSFUL;

        Offset += ChunkLength;
        Buffer += ChunkLength;
        Length -= ChunkLength;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS
AccessVolume(_In_    ULONGLONG Offset,
             _In_    ULONG Length,
             _Inout_ PUCHAR Buffer,
             _In_    BOOLEAN Write)
{
    NTSTATUS Status;
    PUCHAR TemporaryBuffer;

    if (!Length)
        return STATUS_SUCCESS;

    if (!Buffer)
        return STATUS_INVALID_PARAMETER;

    // Optimization: if the buffer is already page aligned, we can call AccessVolumeAligned directly
    if (reinterpret_cast<ULONG_PTR>(Buffer) % PAGE_SIZE == 0)
    {
        return AccessVolumeAligned(Offset, Length, Buffer, Write);
    }

    // Buffer is not page aligned, create a temporary buffer that is page aligned.
    TemporaryBuffer = static_cast<PUCHAR>(VirtualAlloc(NULL,
                                                       Length,
                                                       MEM_RESERVE | MEM_COMMIT,
                                                       PAGE_READWRITE));
    if (!TemporaryBuffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    // On write: copy the contents of the real buffer to the temporary one.
    if (Write)
        CopyMemory(TemporaryBuffer, Buffer, Length);

    // Perform the aligned volume operation.
    Status = AccessVolumeAligned(Offset, Length, TemporaryBuffer, Write);
    if (!NT_SUCCESS(Status))
        goto Done;

    // On read: copy the contents of the temporary buffer into the real buffer.
    if (!Write)
        CopyMemory(Buffer, TemporaryBuffer, Length);

Done:
    DWORD Error = GetLastError();
    VirtualFree(TemporaryBuffer, 0, MEM_RELEASE);
    SetLastError(Error);
    return Status;
}

NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer)
{
    return AccessVolume(Offset, Length, Buffer, FALSE);
}

NTSTATUS
NtfsWriteVolume(_In_    ULONGLONG Offset,
                _In_    ULONG Length,
                _Inout_ PUCHAR Buffer)
{
    return AccessVolume(Offset, Length, Buffer, TRUE);
}

BOOLEAN
NtfsIsNameInExpression(_In_     PUNICODE_STRING Expression,
                       _In_     PUNICODE_STRING Name,
                       _In_     BOOLEAN IgnoreCase,
                       _In_opt_ PWCHAR UpcaseTable)
{
    return pRtlIsNameInExpression(Expression,
                                  Name,
                                  IgnoreCase,
                                  UpcaseTable);
}

#ifdef __cplusplus
}
#endif
