/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

static
BOOLEAN
NtfsIsIrpTopLevel (_In_ PIRP Irp)
{
    PAGED_CODE();

    if (!IoGetTopLevelIrp())
    {
        IoSetTopLevelIrp(Irp);
        return TRUE;
    }

    return FALSE;
}