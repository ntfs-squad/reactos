/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

EXTERN_C
NTSTATUS
NtfsProbePartition(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData);

EXTERN_C
NTSTATUS
NtfsProbePartitionAndOpenVolume(
    _In_ ULONG BytesPerSector,
    _In_ PUCHAR BootSectorData,
    _Out_ void** VolumeOut);
