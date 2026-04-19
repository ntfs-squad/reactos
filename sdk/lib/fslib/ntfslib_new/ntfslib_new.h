// Hack: This whole header is a hack replacement for ntfspch.h

// Hack: This should only be in enviornments/km.cpp
#include <ntifs.h>

// Hack: This should not be in any public header.
#include "ntfs_tags.h"

// Hack: This is a driver-specific setting. Our lib should not care.
extern BOOLEAN gShowMetadataFiles;

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);

#define GetWStrLength(x) ((x) * sizeof(WCHAR))
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((N) - ((N) % (S)))
#define ULONG_ROUND_UP(x) ROUND_UP((x), (sizeof(ULONG)))
#define MAX_SHORTNAME_LENGTH 12
#define FileRef(Key) ((Key)->Entry->Data.Directory.IndexedFile)

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef UINT
typedef unsigned int UINT;
#endif

#include "filerecord/attributes/attributes.h"
#include "ntfsvol/ntfsvol.h"
#include "filerecord/filerecord.h"

#include "mft/mft.h"
#include "lfs/logfile.h"
#include "lfs/usnjrnl.h"
#include "lfs/lfs.h"

#include "btree/btree.h"
#include "btree/directory/directory.h"

#include <debug.h>
