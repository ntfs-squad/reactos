#include "../io/ntfsprocs.h"

#define INDEX_ROOT_SMALL 0x0
#define INDEX_ROOT_LARGE 0x1

#define INDEX_NODE_SMALL 0x0
#define INDEX_NODE_LARGE 0x1

#define NTFS_INDEX_ENTRY_NODE 1
#define NTFS_INDEX_ENTRY_END  2

#define COLLATION_BINARY              0x00
#define COLLATION_FILE_NAME           0x01
#define COLLATION_UNICODE_STRING      0x02
#define COLLATION_NTOFS_ULONG         0x10
#define COLLATION_NTOFS_SID           0x11
#define COLLATION_NTOFS_SECURITY_HASH 0x12
#define COLLATION_NTOFS_ULONGS        0x13

// The beginning and length of an attribute record are always aligned to an 8-byte boundary,
// relative to the beginning of the file record.
#define ATTR_RECORD_ALIGNMENT 8

#define BytesPerCluster(x) x->BytesPerSector * x->SectorsPerCluster
#define BytesPerIndexRecord(x) BytesPerCluster(x) * x->ClustersPerIndexRecord
#define AttributeDataLength(x) x->IsNonResident ? x->NonResident.DataSize : x->Resident.DataLength

typedef struct
{
    NTFSRecordHeader RecordHeader;
    ULONGLONG VCN;
    IndexNodeHeader IndexHeader;
} INDEX_BUFFER, *PINDEX_BUFFER;

typedef struct
{
    union
    {
        struct
        {
            ULONGLONG    IndexedFile;
        } Directory;
        struct
        {
            USHORT    DataOffset;
            USHORT    DataLength;
            ULONG    Reserved;
        } ViewIndex;
    } Data;
    USHORT            Length;
    USHORT            KeyLength;
    USHORT            Flags;
    USHORT            Reserved;
    FileNameEx        FileName;
} INDEX_ENTRY_ATTRIBUTE, *PINDEX_ENTRY_ATTRIBUTE;

struct _BTreeFilenameNode;
typedef struct _BTreeFilenameNode BTreeFilenameNode;

// Keys are arranged in nodes as an ordered, linked list
typedef struct _BTreeKey
{
    struct _BTreeKey *NextKey;
    BTreeFilenameNode *LesserChild;  // Child-Node. All the keys in this node will be sorted before IndexEntry
    PINDEX_ENTRY_ATTRIBUTE IndexEntry;  // must be last member for FIELD_OFFSET
}BTreeKey, *PBTreeKey;

// Every Node is just an ordered list of keys.
// Sub-nodes can be found attached to a key (if they exist).
// A key's sub-node precedes that key in the ordered list.
typedef struct _BTreeFilenameNode
{
    ULONG KeyCount;
    BOOLEAN HasValidVCN;
    BOOLEAN DiskNeedsUpdating;
    ULONGLONG VCN;
    PBTreeKey FirstKey;
} BTreeFilenameNode, *PBTreeFilenameNode;

typedef struct
{
    PBTreeFilenameNode RootNode;
} BTree, *PBTree;