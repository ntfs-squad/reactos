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
#define FindKeyFromFileName(Tree, FileName)\
wcschr(FileName, L'\\') ? \
FindKeyInNode(Tree->RootNode, FileName, (wcschr(FileName, L'\\') - FileName)) :\
FindKeyInNode(Tree->RootNode, FileName, wcslen(FileName))

typedef struct
{
    NTFSRecordHeader RecordHeader;
    ULONGLONG VCN;
    IndexNodeHeader IndexHeader;
} INDEX_BUFFER, *PINDEX_BUFFER;

VOID
PrintAllVCNs(PNTFSVolume Volume,
             PFileRecord File,
             PAttribute Attr,
             ULONG NodeSize);

ULONG GetFileNameAttributeLength(PFileNameEx FileNameAttribute);

VOID
DumpBTreeKey(PBTree Tree,
             PBTreeKey Key,
             ULONG Number,
             ULONG Depth);

VOID
DestroyBTreeNode(PBTreeFilenameNode Node);

VOID
DumpBTreeNode(PBTree Tree,
              PBTreeFilenameNode Node,
              ULONG Number,
              ULONG Depth);

VOID
DestroyBTree(PBTree Tree);

VOID
DumpBTree(PBTree Tree);

VOID
DumpBTreeKey(PBTree Tree,
             PBTreeKey Key,
             ULONG Number,
             ULONG Depth);

PBTreeFilenameNode
CreateBTreeNodeFromIndexNode(PNTFSVolume Volume,
                             PFileRecord File,
                             IndexRootEx* IndexRoot,
                             PAttribute IndexAllocationAttribute,
                             PINDEX_ENTRY_ATTRIBUTE NodeEntry);

NTSTATUS
CreateBTreeFromFile(PFileRecord File,
                    PBTree *NewTree);
PBTreeKey
CreateBTreeKeyFromFilename(ULONGLONG FileReference, PFileNameEx FileNameAttribute);

PBTreeKey
FindKeyInNode(PBTreeFilenameNode Node, PWCHAR FileName, UINT Length);
