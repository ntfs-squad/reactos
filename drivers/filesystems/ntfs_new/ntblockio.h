class  NtBlockIo
{
public:
    NTSTATUS ReadDisk(_In_ PDEVICE_OBJECT DeviceObject,
                      _In_ LONGLONG StartingOffset,
                      _In_ ULONG Length,
                      _In_ ULONG SectorSize,
                      _Inout_  PUCHAR Buffer,
                      _In_ BOOLEAN Override);
    NTSTATUS ReadBlock(_In_    PDEVICE_OBJECT DeviceObject,
                       _In_    ULONG DiskSector,
                       _In_    ULONG SectorCount,
                       _In_    ULONG SectorSize,
                       _Inout_ PUCHAR Buffer,
                       _In_    BOOLEAN Override);
    NTSTATUS DeviceIoControl(_In_    PDEVICE_OBJECT DeviceObject,
                             _In_    ULONG ControlCode,
                             _In_    PVOID InputBuffer,
                             _In_    ULONG InputBufferSize,
                             _Inout_ PVOID OutputBuffer,
                             _Inout_ PULONG OutputBufferSize,
                             _In_    BOOLEAN Override);
        NTSTATUS WriteDisk(); //TODO:
private:
public:
};