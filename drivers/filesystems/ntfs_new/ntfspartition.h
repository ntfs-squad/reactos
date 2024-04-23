
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
    
private:
    NtBlockIo* BlockIo; /* Raw BlockIo - DONT USE */
    USHORT    BytesPerSector;



    char*    OEM_ID;
	UINT16   BYTES_PER_SECTOR;
	UCHAR    SECTORS_PER_CLUSTER;
	UCHAR    MEDIA_DESCRIPTOR;
	UINT16   SECTORS_PER_TRACK;
	UINT16   NUM_OF_HEADS;
	UINT64   SECTORS_IN_VOLUME;
	UINT64   LCN_FOR_MFT;
	UINT64   LCN_FOR_MFT_MIRR;
	UINT32   CLUSTERS_PER_MFT_RECORD;
	UINT32   CLUSTERS_PER_INDEX_RECORD;
	UINT64   VOLUME_SERIAL_NUMBER;
};