#pragma once

class FileContextBlock
{
    PFILE_OBJECT FileObject;

    ULONGLONG MFTIndex;
};

class VolumeContextBlock
{
public:
    ULONG  BytesPerSector;
    UINT8  SectorsPerCluster;
    UINT64 SectorsInVolume;
    UINT64 MFTLCN;
    UINT64 MFTMirrLCN;
    INT32  ClustersPerFileRecord;
    INT32  ClustersPerIndexRecord;
    UINT64 SerialNumber;
};
