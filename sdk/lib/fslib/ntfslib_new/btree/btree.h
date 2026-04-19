/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#define INDEX_ENTRY_NODE 1
#define INDEX_ENTRY_END  2

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
    ULONG       Flags;
} BTreeKey, *PBTreeKey;

typedef struct
{
    NTFSRecordHeader RecordHeader;
    ULONGLONG VCN;
    IndexNodeHeader IndexHeader;
} IndexBuffer, *PIndexBuffer;

typedef class BTree
{
public:
    NTSTATUS ResetCurrentKey();
    // Hack:
    // TODO: Make private when we abandon oldbtreefuncs
    PBTreeNode RootNode;
protected:
    ~BTree();
    PBTreeKey CurrentKey;
} *PBTree;
