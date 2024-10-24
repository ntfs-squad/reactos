/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Justin Miller <justin.miller@reactos.org>
 *              Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 */

#include "io/ntfsprocs.h"

/* *** FILE RECORD IMPLEMENTATIONS *** */
FileRecord::FileRecord(_In_ PNTFSVolume Volume)
{
    this->Volume = Volume;
}