/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new.h"
#include "ntfslib_new_internal.h"

NTSTATUS
Directory::AddFileToDirectory(_In_ PFileNameEx FileToAdd)
{
    /* Algorithm:
     *    - Log to LFS
     *    - Manipulate the btree as needed to add the entry
     *    - Write to disk
     */

    DPRINT1("AddFileToDirectory() called!\n");
    DPRINT1("Logging using LFS is not implemented!\n");
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
Directory::RemoveFileFromDirectory(_In_ PBTreeKey FileToRemove)
{
    /* Algorithm:
     *    - Log to LFS
     *    - Manipulate the btree as needed to remove the entry
     *    - Write to disk
     */

    DPRINT1("RemoveFileFromDirectory() called!\n");
    DPRINT1("Logging using LFS is not implemented!\n");
    return STATUS_NOT_IMPLEMENTED;
}