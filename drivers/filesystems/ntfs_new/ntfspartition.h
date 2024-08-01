#pragma once

class MFT;

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

typedef class NtfsPartition
{
public:
    ULONG  BytesPerSector;
    UINT8  SectorsPerCluster;
    UINT32 ClustersInVolume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    INT8   ClustersPerFileRecord;
    INT8   ClustersPerIndexRecord;
    UINT64 SerialNumber;

    PDEVICE_OBJECT StorageDevice;
    PFILE_OBJECT StreamFileObject;

    ULONG Flags;
    ULONG OpenHandleCount;

    PVPB VolParamBlock;
    MFT* VolMFT;

    ~NtfsPartition();
    NTSTATUS LoadNtfsDevice(_In_ PDEVICE_OBJECT DeviceToMount);
    void     CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject);
    NTSTATUS DumpBlocks(_Inout_ PUCHAR Buffer,
                        _In_    ULONG Lba,
                        _In_    ULONG LbaCount);
    NTSTATUS GetVolumeLabel(_Inout_ PWCHAR VolumeLabel,
                            _Inout_ PUSHORT Length);
    NTSTATUS GetFreeClusters(_Out_ PULONG FreeClusters);
    void RunSanityChecks();
    PFILE_OBJECT PubFileObject;
    PDEVICE_OBJECT PartDeviceObj;
} *PNtfsPartition;

