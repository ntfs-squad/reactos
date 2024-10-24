#include "../io/ntfsprocs.h"

static
void
DumpBTreeNode(PBTreeNode Node,
              UINT Depth);
static
void
DumpBTreeKey(PBTreeKey Key,
             ULONG Depth)
{
    ULONG i;
    for (i = 0; i < Depth; i++)
        DbgPrint("    ");

    if (!(Key->Entry->Flags & INDEX_ENTRY_END))
    {
        UNICODE_STRING FileName;
        FileName.Length = GetFileName(Key)->NameLength * sizeof(WCHAR);
        FileName.MaximumLength = FileName.Length;
        FileName.Buffer = GetFileName(Key)->Name;
        DbgPrint("|===%wZ\n", &FileName);
    }
    else
    {
        DbgPrint("L===(Dummy Key)\n");
    }

    // Is there a child node?
    if (Key->Entry->Flags & INDEX_ENTRY_NODE)
    {
        ASSERT(Key->ChildNode);
        DumpBTreeNode(Key->ChildNode, (Depth + 1));
    }
}

static
void
DumpBTreeNode(PBTreeNode Node,
              UINT Depth)
{
    PBTreeKey CurrentKey;
    CurrentKey = Node->FirstKey;
    while(CurrentKey)
    {
        DumpBTreeKey(CurrentKey, Depth);
        CurrentKey = CurrentKey->NextKey;
    }
}

void
Directory::DumpFileTree()
{
    // Print header
    DbgPrint("Index Root\n");
    DumpBTreeNode(RootNode, 0);
}