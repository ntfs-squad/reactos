#include <ntifs.h>

#define ATTR_END 0xFFFFFFFF

/* Attribute flags */
#define ATTR_COMPRESSED  0x0001
#define ATTR_ENCRYPTED   0x4000
#define ATTR_SPARSE      0x8000

/* Filename flags*/
#define FN_READONLY   0x0001
#define FN_HIDDEN     0x0002
#define FN_SYSTEM     0x0004
#define FN_ARCHIVE    0x0020
#define FN_DEVICE     0x0040
#define FN_TEMP       0x0100
#define FN_SPARSE     0x0200
#define FN_REPARSE    0x0400
#define FN_COMPRESSED 0x0800
#define FN_OFFLINE    0x1000
#define FN_NOTINDEXED 0x2000
#define FN_ENCRYPTED  0x4000

/* Attribute types */
enum AttributeType
{
    StandardInformation = 0x10,
    AttributeList       = 0x20,
    FileName            = 0x30,
    ObjectId            = 0x40,
    SecurityDescriptor  = 0x50,
    VolumeName          = 0x60,
    VolumeInformation   = 0x70,
    Data                = 0x80,
    IndexRoot           = 0x90,
    IndexAllocation     = 0xA0,
    Bitmap              = 0xB0,
    ReparsePoint        = 0xC0,
    EAInformation       = 0xD0,
    EA                  = 0xE0,
    LoggedUtilityStream = 0x100,
};

struct IAttribute
{
    UINT32 AttributeType;             // Offset 0x00, Size 4
    UINT32 Length;                    // Offset 0x04, Size 4
    UINT8  NonResidentFlag;           // Offset 0x08, Size 1
    UINT8  NameLength;                // Offset 0x09, Size 1
    UINT16 NameOffset;                // Offset 0x0A, Size 2
    UINT16 Flags;                     // Offset 0x0C, Size 2
    UINT16 AttributeID;               // Offset 0x0E, Size 2
};

struct ResidentAttribute : IAttribute
{
    /* Value of AttributeOffset should be (2 * NameLength + 0x18).
     * Size of AttributeName should be (2 * NameLength).
     * Attribute contents has an offset of (2 * NameLength + 0x18).
     */
    UINT32 AttributeLength;           // Offset 0x10, Size 4
    UINT16 AttributeOffset;           // Offset 0x14, Size 2
    UINT8  IndexedFlag;               // Offset 0x16, Size 1
};

struct NonResidentAttribute : IAttribute
{
    /* Value of DataRunsOffset should be (2 * NameLength + 0x40).
     * Size of AttributeName should be (2 * NameLength).
     * Data Run has an offset of (2 * NameLength + 0x40).
     */
    UINT64 FirstVCN;               // Offset 0x10, Size 8
    UINT64 LastVCN;                // Offset 0x18, Size 8
    UINT16 DataRunsOffset;         // Offset 0x20, Size 2
    UINT16 CompressionUnitSize;    // Offset 0x20, Size 2
    UINT32 Reserved;               // Offset 0x22, Size 4
    UINT64 AllocatedAttributeSize; // Offset 0x28, Size 8
    UINT64 ActualAttributeSize;    // Offset 0x30, Size 8
    UINT64 InitalizedDataSize;     // Offset 0x38, Size 8
};

struct FileNameAttr : ResidentAttribute
{
    UINT64 ParentDirectory;    // Offset 0x00, Size 8
    UINT64 FileCreation;       // Offset 0x08, Size 8
    UINT64 FileChanged;        // Offset 0x10, Size 8
    UINT64 MftChanged;         // Offset 0x18, Size 8
    UINT64 FileRead;           // Offset 0x20, Size 8
    UINT64 AllocatedSize;      // Offset 0x28, Size 8
    UINT64 RealSize;           // Offset 0x30, Size 8
    UINT32 Flags;              // Offset 0x38, Size 4
    UINT32 Reserved;           // Offset 0x3C, Size 4
    UINT8  FilenameChars;      // Offset 0x40, Size 1
    UINT8  FilenameNamespace;  // Offset 0x41, Size 1
};