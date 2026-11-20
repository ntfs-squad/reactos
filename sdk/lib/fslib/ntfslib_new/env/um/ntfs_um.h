#ifdef __cplusplus
extern "C" {
#endif

/* FileHandle must be opened for synchronous I/O. */
NTSTATUS
NtfsDiskInitializeUm(
    _In_      HANDLE FileHandle,
    _Out_opt_ ULONG* BytesPerSector);

#ifdef __cplusplus
}
#endif
