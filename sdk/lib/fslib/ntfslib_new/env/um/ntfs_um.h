#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
NtfsDiskInitializeUm(
    _In_      HANDLE FileHandle,
    _Out_opt_ ULONG* BytesPerSector);

/* HACK: Don't expose this */
NTSTATUS
NtfsReadVolume(_In_    ULONGLONG Offset,
               _In_    ULONG Length,
               _Inout_ PUCHAR Buffer);

#ifdef __cplusplus
}
#endif