
#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsDiskInitializeKm(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG BytesPerSector);

#ifdef __cplusplus
}
#endif

