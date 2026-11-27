
typedef struct _NtfsDevice NtfsDevice, *PNtfsDevice;

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Out_ PNtfsDevice DeviceOut);

NTSTATUS
AccessDisk(
    _In_ ULONG MajorFunction,
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONGLONG Offset,
    _In_ ULONG Length,
    _Inout_ PUCHAR Buffer);

#ifdef __cplusplus
}
#endif

