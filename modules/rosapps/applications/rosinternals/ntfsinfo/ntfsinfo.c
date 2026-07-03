

#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <ntfs_um.h>
#include <ntfslib_new.h>
// Hack: We shouldn't be defining this here.
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) ((Status) >= 0)
#endif

typedef struct _NTFS_INFORMATION
{
    ULONGLONG VolumeSize;
    ULONG SectorsPerCluster;
    ULONGLONG TotalClusters;
    ULONGLONG FreeClusters;
    ULONG BytesPerSector;
    ULONG BytesPerCluster;
    ULONG ClustersPerMftRecord;
    ULONGLONG MftSize;
    ULONGLONG MftStartCluster;
    ULONGLONG MftZoneStartCluster;
    ULONGLONG MftZoneEndCluster;
    ULONGLONG MftMirrorStartCluster;
    ULONGLONG _MftSize;
    ULONGLONG _MftMirrSize;
    ULONGLONG _LogFileSize;
    ULONGLONG _VolumeSize;
    ULONGLONG _AttrDefSize;
    ULONGLONG _DotSize;
    ULONGLONG _BitmapSize;
    ULONGLONG _BootSize;
    ULONGLONG _BadClusSize;
    ULONGLONG _UpCaseSize;
} NTFS_INFORMATION, *PNTFS_INFORMATION;

void PrintBanner()
{
    printf("\nNtfsInfo v1.0 - NTFS Information Dump\n");
    printf("Copyright (C) 2026 Carl Bialorucki\n");
    printf("ReactOS - reactos.org\n\n");
}

void PrintUsage()
{
    printf("Usage: ntfsinfo [-nobanner] <volume>\n");
    printf("-nobanner    Do not display the startup banner and copyright message.\n\n");
    printf("Volume formats:\n");
    printf("    %-32sDrive letter\n", "C:");
    printf("    %-32sNT native path\n", "\\Device\\Harddisk0\\Partition1");
}

void PrintTableHeader(const char* header)
{
    unsigned int width = strlen(header);

    printf("\n%s\n", header);
    for (unsigned int i = 0; i < width; i++)
        putchar('-');
    putchar('\n');
}

void PrintRow(const char* label, const char* value)
{
    printf("%-23s: %s\n", label, value);
}

char* FormatSize(ULONGLONG size)
{
    static char buffer[50];
    if (size < 1024)
        sprintf(buffer, "%llu B", size);
    else if (size < 1024 * 1024)
        sprintf(buffer, "%llu KB", size / 1024);
    else if (size < 1024ull * 1024 * 1024)
        sprintf(buffer, "%llu MB", size / (1024 * 1024));
    else if (size < 1024ull * 1024 * 1024 * 1024)
        sprintf(buffer, "%llu GB", size / (1024 * 1024 * 1024));
    else
        sprintf(buffer, "%llu TB", size / (1024ull * 1024 * 1024 * 1024));
    return buffer;
}

char* FormatSizeWithPercentDrive(ULONGLONG size, ULONGLONG total)
{
    static char buffer[100];
    sprintf(buffer, "%s (%llu%% of drive)", FormatSize(size), (size * 100) / total);
    return buffer;
}

char* FormatNumber(ULONGLONG Number)
{
    static char buffer[50];
    sprintf(buffer, "%llu", Number);
    return buffer;
}

char* FormatRange(ULONGLONG Number1, ULONGLONG Number2)
{
    static char buffer[50];
    sprintf(buffer, "%llu - %llu", Number1, Number2);
    return buffer;
}

void PrintNtfsInformation(PNTFS_INFORMATION NtfsInformation)
{
    PrintTableHeader("Volume size");
    PrintRow("Volume size", FormatSize(NtfsInformation->VolumeSize));
    PrintRow("Total sectors", FormatNumber(NtfsInformation->SectorsPerCluster * NtfsInformation->TotalClusters));
    PrintRow("Total clusters", FormatNumber(NtfsInformation->TotalClusters));
    PrintRow("Free clusters", FormatNumber(NtfsInformation->FreeClusters));
    PrintRow("Free space", FormatSizeWithPercentDrive(NtfsInformation->FreeClusters * NtfsInformation->BytesPerCluster,
                                                      NtfsInformation->VolumeSize));

    PrintTableHeader("Allocation size");
    PrintRow("Bytes per sector", FormatNumber(NtfsInformation->BytesPerSector));
    PrintRow("Bytes per cluster", FormatNumber(NtfsInformation->BytesPerCluster));
    PrintRow("Bytes per MFT record", FormatNumber(NtfsInformation->BytesPerCluster * NtfsInformation->ClustersPerMftRecord));
    PrintRow("Clusters per MFT record", FormatNumber(NtfsInformation->ClustersPerMftRecord));

    PrintTableHeader("MFT information");
    PrintRow("MFT size", FormatSizeWithPercentDrive(NtfsInformation->MftSize, NtfsInformation->VolumeSize));
    PrintRow("MFT start cluster", FormatNumber(NtfsInformation->MftStartCluster));
    PrintRow("MFT zone clusters", FormatRange(NtfsInformation->MftZoneStartCluster,
                                              NtfsInformation->MftZoneEndCluster));
    PrintRow("MFT zone size", FormatSizeWithPercentDrive(NtfsInformation->MftZoneEndCluster - NtfsInformation->MftZoneStartCluster,
                                                         NtfsInformation->TotalClusters));
    PrintRow("MFT mirror start", FormatNumber(NtfsInformation->MftMirrorStartCluster));

    PrintTableHeader("Metadata files");
    PrintRow("$MFT", FormatSize(NtfsInformation->_MftSize));
    PrintRow("$MFTMirr", FormatSize(NtfsInformation->_MftMirrSize));
    PrintRow("$LogFile", FormatSize(NtfsInformation->_LogFileSize));
    PrintRow("$Volume", FormatSize(NtfsInformation->_VolumeSize));
    PrintRow("$AttrDef", FormatSize(NtfsInformation->_AttrDefSize));
    PrintRow(".", FormatSize(NtfsInformation->_DotSize));
    PrintRow("$Bitmap", FormatSize(NtfsInformation->_BitmapSize));
    PrintRow("$Boot", FormatSize(NtfsInformation->_BootSize));
    PrintRow("$BadClus", FormatSize(NtfsInformation->_BadClusSize));
    PrintRow("$UpCase", FormatSize(NtfsInformation->_UpCaseSize));
}

void PrintLastError(const char* Prefix)
{
    DWORD Error = GetLastError();
    LPSTR MessageBuffer = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   NULL,
                   Error,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPSTR)&MessageBuffer,
                   0,
                   NULL);
    printf("%s: %s\n", Prefix, MessageBuffer);
    LocalFree(MessageBuffer);
}

HANDLE OpenFromDriveLetter(WCHAR DriveLetter)
{
    WCHAR path[32];

    swprintf(path, L"\\\\.\\%wc:", DriveLetter);
    return CreateFileW(path,
                       GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);
}

HANDLE OpenFromNtNativePath(const WCHAR *NtPath)
{
    WCHAR path[MAX_PATH];

    swprintf(path, L"\\\\?\\GLOBALROOT%s", NtPath);
    return CreateFileW(path,
                       GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);
}

int wmain(int argc, wchar_t* argv[])
{
    NTSTATUS Status;
    NTFS_INFORMATION NtfsInformation;
    HANDLE VolumeHandle;
    const BOOL NoBanner = (argc > 1) && (_wcsicmp(argv[1], L"-nobanner") == 0);
    ULONG BytesPerSector;
    PNtfsVolume VolumeObject;
    // TODO: Maybe this should go in NtfsProbePartition?
    PUCHAR BootSectorData;

    if (!NoBanner)
        PrintBanner();

    // If no partition was specified, print usage and exit
    if (argc < (NoBanner ? 3 : 2))
    {
        PrintUsage();
        return 1;
    }

    if (iswalpha(argv[NoBanner ? 2 : 1][0]))
        VolumeHandle = OpenFromDriveLetter(argv[NoBanner ? 2 : 1][0]);

    else
        VolumeHandle = OpenFromNtNativePath(argv[NoBanner ? 2 : 1]);

    if (VolumeHandle == INVALID_HANDLE_VALUE)
    {
        PrintLastError("Error opening volume");
        return 1;
    }

    // We now have a file handle to the volume. Let's get the NTFS information.
    Status = NtfsDiskInitializeUm(VolumeHandle,
                                  &BytesPerSector);
    if (!NT_SUCCESS(Status))
    {
        PrintLastError("Error initializing disk routines");
        return 1;
    }

    // // Get the boot sector data
    BootSectorData = (PUCHAR)malloc(BytesPerSector);
    if (!BootSectorData)
    {
        PrintLastError("Error allocating boot sector data");
        return 1;
    }

    Status = NtfsReadVolume(0, BytesPerSector, BootSectorData);
    if (!NT_SUCCESS(Status))
    {
        PrintLastError("Error reading boot sector data");
        free(BootSectorData);
        return 1;
    }

    Status = NtfsProbePartitionAndOpenVolume(BytesPerSector,
                                             BootSectorData,
                                             &VolumeObject);

    // If the specificed volume is not NTFS, print an error and exit.
    if (!NT_SUCCESS(Status))
    {
        PrintLastError("Error obtaining NTFS information");
        return 1;
    }

    free(BootSectorData);
    // Let's find the NTFS information and print it.
    // HACK: This is just mock data for now.
    NtfsInformation.VolumeSize = 475967ull * 1024 * 1024;
    NtfsInformation.SectorsPerCluster = 8;
    NtfsInformation.TotalClusters = 192881;
    NtfsInformation.FreeClusters = 192881;
    NtfsInformation.BytesPerSector = BytesPerSector;
    NtfsInformation.BytesPerCluster = 512 * 8;
    NtfsInformation.ClustersPerMftRecord = 1;
    NtfsInformation.MftSize = 192881 * 512;
    NtfsInformation.MftStartCluster = 1;
    NtfsInformation.MftZoneStartCluster = 1;
    NtfsInformation.MftZoneEndCluster = 1;
    NtfsInformation.MftMirrorStartCluster = 2;  
    NtfsInformation._MftSize = 0;
    NtfsInformation._MftMirrSize = 0;
    NtfsInformation._LogFileSize = 0;
    NtfsInformation._VolumeSize = 0;
    NtfsInformation._AttrDefSize = 0;
    NtfsInformation._DotSize = 0;
    NtfsInformation._BitmapSize = 0;
    NtfsInformation._BootSize = 0;
    NtfsInformation._BadClusSize = 0;
    NtfsInformation._UpCaseSize = 0;
    PrintNtfsInformation(&NtfsInformation);
    return 0;
}
