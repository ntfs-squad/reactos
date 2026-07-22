#include <ntfslib_new.h>
#include <ntfslib_new_internal.h>

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType)
{

    Size = (Size != 0) ? Size : 1;
    void* pObject = NtfsAllocatePoolWithTag(PoolType, Size, TAG_NTFS);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType)
{

    Size = (Size != 0) ? Size : 1;

    void* pObject = NtfsAllocatePoolWithTag(PoolType, Size, TAG_NTFS);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag)
{

    Size = (Size != 0) ? Size : 1;
    void* pObject = NtfsAllocatePoolWithTag(PoolType, Size, Tag);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag)
{

    Size = (Size != 0) ? Size : 1;

    void* pObject = NtfsAllocatePoolWithTag(PoolType, Size, Tag);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void __cdecl operator delete(void* pObject)
{
    if (pObject != NULL)
        NtfsFreePool(pObject);
}

void __cdecl operator delete(void* pObject, size_t s)
{
    UNREFERENCED_PARAMETER( s );
    ::operator delete( pObject );
}

void __cdecl operator delete[](void* pObject)
{
    if (pObject != NULL)
        NtfsFreePool(pObject);
}
