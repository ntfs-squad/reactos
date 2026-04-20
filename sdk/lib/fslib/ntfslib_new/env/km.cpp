/*
 * PROJECT:     ReactOS NTFS library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Kernelmode glue
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include <ntifs.h>

#ifndef TAG_NTFS
#define TAG_NTFS 'NTFS'
#endif

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType)
{

    Size = (Size != 0) ? Size : 1;
    void* pObject = ExAllocatePoolWithTag(PoolType, Size, TAG_NTFS);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType)
{

    Size = (Size != 0) ? Size : 1;

    void* pObject = ExAllocatePoolWithTag(PoolType, Size, TAG_NTFS);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag)
{

    Size = (Size != 0) ? Size : 1;
    void* pObject = ExAllocatePoolWithTag(PoolType, Size, Tag);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag)
{

    Size = (Size != 0) ? Size : 1;

    void* pObject = ExAllocatePoolWithTag(PoolType, Size, Tag);

#if DBG
    if (pObject != NULL)
        RtlFillMemory(pObject, Size, 0xCD);
#endif // DBG

    return pObject;
}

void __cdecl operator delete(void* pObject)
{
    if (pObject != NULL)
        ExFreePool(pObject);
}

void __cdecl operator delete(void* pObject, size_t s)
{
    UNREFERENCED_PARAMETER( s );
    ::operator delete( pObject );
}

void __cdecl operator delete[](void* pObject)
{
    if (pObject != NULL)
        ExFreePool(pObject);
}
