/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfslib_new_internal.h"

EXTERN_C
NTSTATUS
NtfsFormatPartition(
    _In_ BOOL QuickFormat
)
{
    // Callback for progress indicator?

    // Get system time for format

    // Compute on disk layout

    // Kill bootsectors

    // Write metadata files

    // Rebuild bootsectors

    // Cleanup

    // Done
}
