class NtfsPartition
{
public:
    NtfsPartition();
    ~NtfsPartition();
    void CreateFileObject(_In_ PDEVICE_OBJECT DeviceObject);
PFILE_OBJECT PubFileObject;
PDEVICE_OBJECT PubDeviceObject;
private:
};