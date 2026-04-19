// This whole header is a hack replacement for ntfspch.h

#include <ntifs.h>

void* __cdecl operator new(size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType);
void* __cdecl operator new[](size_t Size, POOL_TYPE PoolType, ULONG Tag);

#ifndef TAG_NTFS
#define TAG_NTFS 'NTFS'
#endif
#ifndef TAG_MFT
#define TAG_MFT '$MFT'
#endif
#ifndef TAG_FILE_RECORD
#define TAG_FILE_RECORD 'FREC'
#endif
#ifndef TAG_DATA_RUN
#define TAG_DATA_RUN 'DTRN'
#endif
#ifndef TAG_LOG_FILE_SERVICE
#define TAG_LOG_FILE_SERVICE 'LgFS'
#endif
#ifndef TAG_BTREE
#define TAG_BTREE 'BTRE'
#endif

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

extern BOOLEAN gShowMetadataFiles;

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
