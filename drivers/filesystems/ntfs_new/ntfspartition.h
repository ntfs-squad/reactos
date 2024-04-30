class NtfsPartition
{
public:
    NtfsPartition(PDEVICE_OBJECT DeviceToMount);
    ~NtfsPartition();
    void CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject);
    NTSTATUS NtfsPartition::DumpBlocks(_Inout_ PUCHAR Buffer,
                                       _In_    ULONG Lba,
                                       _In_    ULONG LbaCount);
    void RunSanityChecks();
    PFILE_OBJECT PubFileObject;
    PDEVICE_OBJECT PartDeviceObj;
    //NTSTATUS NtfsPartition::PrepareMount();

private:
    NtBlockIo* BlockIo; /* Raw BlockIo - DONT USE */
    USHORT    BytesPerSector;

    UINT8  SectorsPerCluster;
    UINT64 SectorsInVolume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    UINT32 ClustersPerFileRecord;
    UINT32 ClustersPerIndexRecord;
    UINT64 SerialNumber;

    PVPB VolumeParameterBlock;



    /* Used for debug print */
    char*    OEM_ID;
	UCHAR    MEDIA_DESCRIPTOR;
	UINT16   SECTORS_PER_TRACK;
	UINT16   NUM_OF_HEADS;
};