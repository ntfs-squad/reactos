#define INDEX_ENTRY_NODE 1
#define INDEX_ENTRY_END  2

#define GetFileName(Key) \
((PFileNameEx)((Key)->Entry->IndexStream))

#define FindKeyFromFileName(RootNode, FileName)\
wcschr(FileName, L'\\') ? \
FindKeyInNode(RootNode, FileName, (wcschr(FileName, L'\\') - FileName)) :\
FindKeyInNode(RootNode, FileName, wcslen(FileName))

struct _BTreeNode;
struct _BTreeKey;

typedef struct _BTreeNode
{
    ULONGLONG  VCN;
    _BTreeKey* FirstKey;
} BTreeNode, *PBTreeNode;

typedef struct _BTreeKey
{
    _BTreeKey*  ParentNodeKey; // Used to get entry listings linearly.
    _BTreeNode* ChildNode;
    _BTreeKey*  NextKey;
    PIndexEntry Entry;
} BTreeKey, *PBTreeKey;

typedef struct
{
    NTFSRecordHeader RecordHeader;
    ULONGLONG VCN;
    IndexNodeHeader IndexHeader;
} IndexBuffer, *PIndexBuffer;

NTSTATUS
CreateRootNode(_In_  PFileRecord File,
               _Out_ PBTreeNode *NewRootNode);
NTSTATUS
DestroyBTreeNode(PBTreeNode Node);

NTSTATUS
DestroyBTreeKey(PBTreeKey Key);

PBTreeKey
FindKeyInNode(PBTreeNode Node,
              PWCHAR FileName,
              UINT Length);

void
DumpBTreeRootNode(PBTreeNode RootNode);

void
DumpBTreeKey(PBTreeKey Key,
             ULONG Depth);
