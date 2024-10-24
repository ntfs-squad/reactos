/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#define INDEX_ENTRY_NODE 1
#define INDEX_ENTRY_END  2

#define GetVCN(NodeKey) \
(PULONGLONG)(NodeKey->Entry + NodeKey->Entry->EntryLength - sizeof(ULONGLONG))

// Calculates start of Index Buffer relative to the index allocation, given the node's VCN
#define GetAllocationOffsetFromVCN(Volume, IndexBufferSize, VCN) \
(IndexBufferSize < BytesPerCluster(Volume)) ? \
(VCN * (Volume->BytesPerSector)) : \
(VCN * BytesPerCluster(Volume))

#define BytesPerIndexRecord(Volume) \
(BytesPerCluster(Volume) * Volume->ClustersPerIndexRecord)

struct _BTreeNode;
struct _BTreeKey;

typedef struct _BTreeNode
{
    ULONGLONG  VCN;
    _BTreeKey* FirstKey;
} BTreeNode, *PBTreeNode;

typedef struct _BTreeKey
{
    _BTreeKey*  ParentNodeKey; // Used to get entries linearly.
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

class BTree
{
public:
    NTSTATUS ResetCurrentKey();
protected:
    ~BTree();
    PBTreeNode RootNode;
    PBTreeKey CurrentKey;
};
