
#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG SectorBytes);

#ifdef __cplusplus
}
#endif

