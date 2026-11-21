#pragma once

/* Enum values matches real POOL_TYPE so it can be cast as POOL_TYPE in
 * kernelmode while avoiding including POOL_TYPE definition in usermode. */
typedef enum _NTFS_POOL
{
    NtfsNonPagedPool = 0,
    NtfsPagedPool = 1
} NTFS_POOL;

#ifdef __cplusplus
extern "C" {
#endif

void*
NtfsAllocatePoolWithTag(_In_ NTFS_POOL PoolType,
                        _In_ size_t Size,
                        _In_ ULONG Tag);

void
NtfsFreePool(_In_ void* pObject);

void
NtfsFillMemory(_In_ PVOID Buffer,
               _In_ size_t Size,
               _In_ UCHAR Value);

BOOLEAN
NtfsIsNameInExpression(_In_     PUNICODE_STRING Expression,
                       _In_     PUNICODE_STRING Name,
                       _In_     BOOLEAN IgnoreCase,
                       _In_opt_ PWCHAR UpcaseTable);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void* __cdecl operator new(size_t Size, NTFS_POOL PoolType);
void* __cdecl operator new(size_t Size, NTFS_POOL PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, NTFS_POOL PoolType);
void* __cdecl operator new[](size_t Size, NTFS_POOL PoolType, ULONG Tag);
#endif
