// io/ntfsinit.cpp

_Function_class_(DRIVER_UNLOAD)
EXTERN_C
VOID
NTAPI
NtfsUnload(_In_ _Unreferenced_parameter_ PDRIVER_OBJECT DriverObject);

EXTERN_C
NTSTATUS
NTAPI
DriverEntry(_In_ PDRIVER_OBJECT DriverObject,
            _In_ PUNICODE_STRING RegistryPath);

_Function_class_(IRP_MJ_CLEANUP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCleanup (_In_    PDEVICE_OBJECT VolumeDeviceObject,
                _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_LOCK_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdLockControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                   _Inout_ PIRP Irp)

_Function_class_(IRP_MJ_DEVICE_CONTROL)
_Function_class_(DRIVER_DISPATCH);
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDeviceControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SHUTDOWN)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdShutdown (_In_    PDEVICE_OBJECT VolumeDeviceObject,
                 _Inout_ PIRP Irp);
// io/fastio
BOOLEAN NTAPI
NtfsAcqLazyWrite(PVOID Context,
                 BOOLEAN Wait);

VOID NTAPI
NtfsRelLazyWrite(PVOID Context);

BOOLEAN NTAPI
NtfsAcqReadAhead(PVOID Context,
                 BOOLEAN Wait);

VOID NTAPI
NtfsRelReadAhead(PVOID Context);

// FastIo function declarations
BOOLEAN NTAPI
NtfsFastIoCheckIfPossible(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ BOOLEAN Wait,
    _In_ ULONG LockKey,
    _In_ BOOLEAN CheckForReadOperation,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoRead(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ BOOLEAN Wait,
    _In_ ULONG LockKey,
    _Out_ PVOID Buffer,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoWrite(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ BOOLEAN Wait,
    _In_ ULONG LockKey,
    _In_ PVOID Buffer,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoQueryBasicInfo(
    _In_ PFILE_OBJECT FileObject,
    _In_ BOOLEAN Wait,
    _Out_ PFILE_BASIC_INFORMATION Buffer,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoQueryStandardInfo(
    _In_ PFILE_OBJECT FileObject,
    _In_ BOOLEAN Wait,
    _Out_ PFILE_STANDARD_INFORMATION Buffer,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoQueryNetworkOpenInfo(
    _In_ PFILE_OBJECT FileObject,
    _In_ BOOLEAN Wait,
    _Out_ PFILE_NETWORK_OPEN_INFORMATION Buffer,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoQueryDirectory(
    _In_ PFILE_OBJECT FileObject,
    _In_ BOOLEAN Wait,
    _In_ PVOID Buffer,
    _In_ ULONG Length,
    _In_ FILE_INFORMATION_CLASS FileInformationClass,
    _In_ BOOLEAN ReturnSingleEntry,
    _In_opt_ PUNICODE_STRING FileName,
    _In_ BOOLEAN RestartScan,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoAcquireFile(
    _In_ PFILE_OBJECT FileObject);

BOOLEAN NTAPI
NtfsFastIoReleaseFile(
    _In_ PFILE_OBJECT FileObject);

BOOLEAN NTAPI
NtfsFastIoDetachDevice(
    _In_ PDEVICE_OBJECT SourceDevice,
    _In_ PDEVICE_OBJECT TargetDevice);

BOOLEAN NTAPI
NtfsFastIoQueryOpen(
    _In_ PIRP Irp,
    _Out_ PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoPrepareMdlWrite(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ ULONG LockKey,
    _Out_ PMDL *MdlChain,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoMdlWriteComplete(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ PMDL MdlChain,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoReadCompressed(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ ULONG LockKey,
    _Out_ PVOID Buffer,
    _Out_ PMDL *MdlChain,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoWriteCompressed(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ ULONG LockKey,
    _In_ PVOID Buffer,
    _Out_ PMDL *MdlChain,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoMdlRead(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ ULONG LockKey,
    _Out_ PMDL *MdlChain,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoMdlReadComplete(
    _In_ PFILE_OBJECT FileObject,
    _In_ PMDL MdlChain,
    _In_ PDEVICE_OBJECT DeviceObject);

BOOLEAN NTAPI
NtfsFastIoAcquireFileForNtCreateSection(
    _In_ PFILE_OBJECT FileObject);

BOOLEAN NTAPI
NtfsFastIoReleaseFileForNtCreateSection(
    _In_ PFILE_OBJECT FileObject);

// io/fsctrl
_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsCommonFileSystemControl(_In_ PIRP Irp);

_Function_class_(IRP_MJ_FILE_SYSTEM_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFileSystemControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                         _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_FLUSH_BUFFERS)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdFlushBuffers (_In_    PDEVICE_OBJECT VolumeDeviceObject,
                     _Inout_ PIRP Irp);

// io/create.cpp
_Function_class_(IRP_MJ_CREATE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdCreate(_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

// io/close.cpp
_Function_class_(IRP_MJ_CLOSE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdClose (_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

// io/read.cpp
_Function_class_(IRP_MJ_READ)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdRead(_In_    PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

// io/write.cpp
_Function_class_(IRP_MJ_WRITE)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdWrite (_In_    PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

// io/fileinfo.cpp

_Function_class_(IRP_MJ_QUERY_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_DIRECTORY_CONTROL)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdDirectoryControl(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                        _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SET_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                      _Inout_ PIRP Irp);
// io/ea.cpp
_Function_class_(IRP_MJ_QUERY_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryEa(_In_   PDEVICE_OBJECT VolumeDeviceObject,
              _Inout_ PIRP Irp);

_Function_class_(IRP_MJ_SET_EA)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetEa(_In_   PDEVICE_OBJECT VolumeDeviceObject,
            _Inout_ PIRP Irp);

// io/vol.cpp
_Function_class_(IRP_MJ_QUERY_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdQueryVolumeInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                              _Inout_ PIRP Irp);

_Requires_lock_held_(_Global_critical_region_)
EXTERN_C
NTSTATUS
NtfsMountVolume(IN PDEVICE_OBJECT TargetDeviceObject,
                IN PVPB Vpb,
                IN PDEVICE_OBJECT FsDeviceObject);

_Function_class_(IRP_MJ_SET_VOLUME_INFORMATION)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdSetVolumeInformation(_In_    PDEVICE_OBJECT VolumeDeviceObject,
                            _Inout_ PIRP Irp);
// io/pnp.cpp
_Function_class_(IRP_MJ_PNP)
_Function_class_(DRIVER_DISPATCH)
EXTERN_C
NTSTATUS
NTAPI
NtfsFsdPnp(_In_    PDEVICE_OBJECT VolumeDeviceObject,
           _Inout_ PIRP Irp);

// io/ntblockio.cpp
NTSTATUS
ReadDisk(_In_    PDEVICE_OBJECT DeviceToRead,
         _In_    ULONGLONG Offset,
         _In_    ULONG Length,
         _Inout_ PUCHAR Buffer);

NTSTATUS
WriteDisk(_In_    PDEVICE_OBJECT DeviceToWrite,
          _In_    ULONGLONG Offset,
          _In_    ULONG Length,
          _In_    PUCHAR Buffer);

NTSTATUS
DeviceIoControl(_In_    PDEVICE_OBJECT DeviceObject,
                _In_    ULONG ControlCode,
                _In_    PVOID InputBuffer,
                _In_    ULONG InputBufferSize,
                _Inout_ PVOID OutputBuffer,
                _Inout_ PULONG OutputBufferSize,
                _In_    BOOLEAN Override);
