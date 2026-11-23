
#define NTFS_KERNEL_MODE

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG SectorBytes);

NTSTATUS
AccessDisk(
    _In_ ULONG MajorFunction,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONGLONG Offset,
    _In_ ULONG Length,
    _Inout_ PUCHAR Buffer);

NTSTATUS
AccessVolume(
    _In_ ULONG MajorFunction,
    _In_ ULONGLONG Offset,
    _In_ ULONG Length,
    _Inout_ PUCHAR Buffer);

#ifdef __cplusplus
}
#endif

