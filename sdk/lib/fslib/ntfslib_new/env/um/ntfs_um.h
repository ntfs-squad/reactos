typedef struct _NtfsDevice NtfsDevice, *PNtfsDevice;

#ifdef __cplusplus
extern "C" {
#endif

/* FileHandle must be opened for synchronous I/O. */
NTSTATUS
NtfsDiskInitializeUm(
    _In_ HANDLE FileHandle,
    _Out_ PNtfsDevice DeviceOut);

#ifdef __cplusplus
}
#endif
