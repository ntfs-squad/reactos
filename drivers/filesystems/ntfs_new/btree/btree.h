/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
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

class BTree
{
public:
    NTSTATUS ResetCurrentKey();
protected:
    ~BTree();
    PBTreeNode RootNode;
    PBTreeKey CurrentKey;
};
