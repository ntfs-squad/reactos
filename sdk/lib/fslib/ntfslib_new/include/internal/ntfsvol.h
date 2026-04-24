/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#define BytesPerCluster(Volume) (Volume->BytesPerSector * Volume->SectorsPerCluster)

#pragma pack(1)
struct BootSector
{
    UCHAR  JumpInstruction[3];     // Offset 0x00, Size 3
    UCHAR  OEM_ID[8];              // Offset 0x03, Size 8
    UINT16 BytesPerSector;         // Offset 0x0B, Size 2
    UINT8  SectorsPerCluster;      // Offset 0x0D, Size 1
    UCHAR  Reserved0[7];           // Offset 0x0E, Size 7
    UINT8  MediaDescriptor;        // Offset 0x15, Size 1
    UCHAR  Reserved1[2];           // Offset 0x16, Size 2
    UINT16 SectorsPerTrack;        // Offset 0x18, Size 2
    UINT16 NumberOfHeads;          // Offset 0x1A, Size 2
    UCHAR  Reserved2[4];           // Offset 0x1C, Size 4
    UCHAR  Reserved3[4];           // Offset 0x20, Size 4
    UINT32 Unknown;                // Offset 0x24, Size 4
    UINT64 SectorsInVolume;        // Offset 0x28, Size 8
    UINT64 MFTLCN;                 // Offset 0x30, Size 8
    UINT64 MFTMirrLCN;             // Offset 0x38, Size 8
    INT8   ClustersPerFileRecord;  // Offset 0x40, Size 4
    UCHAR  Reserved4[3];
    INT8   ClustersPerIndexRecord; // Offset 0x44, Size 4
    UCHAR  Reserved5[3];
    UINT64 SerialNumber;           // Offset 0x48, Size 8
    UINT32 Checksum;               // Offset 0x50, Size 4
    UCHAR  BootStrap[426];
    USHORT EndSector;
};

typedef struct BootSector BootSector;
typedef struct BootSector* PBootSector;

#ifdef __cplusplus

typedef class Volume
{
public:
    ULONG  BytesPerSector;
    UINT8  SectorsPerCluster;
    UINT32 ClustersInVolume;
    INT8   ClustersPerIndexRecord;
    UINT64 SerialNumber;
    USHORT NtfsMajorVersion;
    USHORT NtfsMinorVersion;
    ULONG Flags;
    ULONG OpenHandleCount;
    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;
    PVPB VolParamBlock;
    PFILE_OBJECT PubFileObject;
    PDEVICE_OBJECT PartDeviceObj;
    class MasterFileTable* MFT;
    class LogFileService* LFS;
    BOOLEAN IsReadOnly = FALSE;

    ~Volume();
    
    /**
     * Gets an attribute type value from the name of the attribute. This
     * performs a lookup against the $AttrDef metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    GetAttributeTypeFromName(_In_  PWSTR AttributeTypeName,
                             _Out_ AttributeType* Type);

    /**
     * Gets the number of free clusters in the volume. This reads from the
     * $Bitmap metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    GetFreeClusters(_Out_ PLARGE_INTEGER FreeClusters);

    /**
     * Converts a null-terminated 16-bit string to uppercase using the
     * code page stored on the volume. This reads from the $UpCase metadata
     * file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    UpcaseWideString(_Inout_ PWSTR WideString,
                     _In_    ULONG Length);

    /**
     * Gets the volume label as a 16-bit string and its length in bytes.
     * This reads from the $Volume metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    GetVolumeLabel(_Inout_ PWSTR   VolumeLabel,
                   _Inout_ PUSHORT Length);

    /**
     * Sets the volume label from a 16-bit string and its length in bytes.
     * This writes to the $Volume metadata file on the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS
    SetVolumeLabel(_In_ PWSTR VolumeLabel,
                   _In_ ULONG Length);

    /**
     * Copies a specified number of bytes into a buffer from the volume at a
     * given offset. The offset and length do not have to be sector aligned.
     *
     * @param Offset
     * The offset, in bytes, to begin copying data from the volume.
     *
     * @param Length
     * The number of bytes to copy from the volume.
     *
     * @param Buffer
     * The buffer to copy data from the volume into.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS ReadVolume(_In_    ULONGLONG Offset,
                        _In_    ULONG Length,
                        _Inout_ PUCHAR Buffer);

    /**
     * Writes a specified number of bytes to the volume at a given offset. The
     * offset and length do not have to be sector aligned.
     *
     * @param Offset
     * The offset, in bytes, to begin writing data to the volume.
     *
     * @param Length
     * The number of bytes to write to the volume.
     *
     * @param Buffer
     * The buffer containing the data to write to the volume.
     *
     * @return
     * STATUS_SUCCESS if successful.
     */
    NTSTATUS WriteVolume(_In_    ULONGLONG Offset,
                         _In_    ULONG Length,
                         _Inout_ PUCHAR Buffer);

    NTSTATUS
    Initialize(_In_ PUCHAR BootSectorData);

    NTSTATUS
    GetADSPreference(_In_  PFILE_OBJECT FileObj,
                     _Out_ AttributeType* RequestedType,
                     _Out_ PWSTR* RequestedStream);

    // ./sanity.cpp
    // These functions will likely be removed before the driver is released.
    void RunSanityChecks();
    void SanityCheckBlockIO();

} *PVolume;

#endif // __cplusplus
