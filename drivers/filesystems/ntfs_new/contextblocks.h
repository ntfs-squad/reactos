#pragma once
#include <ntifs.h>

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
    UINT32 ClustersPerFileRecord;
    UINT32 ClustersPerIndexRecord;
    UINT64 SerialNumber;
};
