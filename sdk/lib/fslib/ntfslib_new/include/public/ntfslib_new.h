// Hack: This whole header is a hack replacement for ntfspch.h

// Hack: This should only be in enviornments/km.cpp
#include <ntifs.h>
// TODO: Maybe we should remove this if NTFS_DEBUG isn't defined?
#include <debug.h>

#ifdef __cplusplus
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);
extern "C" {
    // Hack: This is a driver-specific setting. Our lib should not care.
    extern BOOLEAN gShowMetadataFiles;
}
#endif

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define FileRef(Key) ((Key)->Entry->Data.Directory.IndexedFile)

#ifndef UINT
typedef unsigned int UINT;
#endif

#include "ntfs_tags.h" // Hack: this should be private or in *km target.
#include "attributes.h"
#include "ntfsvol.h"
#include "filerecord.h"
#include "mft.h"
#include "lfs.h"
#include "btree.h"
#include "capi.h"
#include "dbg.h"
