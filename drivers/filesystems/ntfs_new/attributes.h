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

/* Volume Information Flags */
#define VOL_DIRTY          0x0001
#define VOL_RESIZE_LOG     0x0002
#define VOL_UPGRADE_ON_MNT 0x0004
#define VOL_USN_DEL_INPROG 0x0010
#define VOL_REPAIR_OBJ_IDS 0x0020
#define VOL_CHECKED        0x8000

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

struct IndexNodeHeader
{
    UINT32 IndexOffset;     // Offset 0x00, Size 4
    UINT32 TotalIndexSize;  // Offset 0x04, Size 4
    UINT32 AllocNodeSize;   // Offset 0x08, Size 4
    UINT8  NodeFlags;       // Offset 0x0C, Size 1
    UCHAR  Padding[3];      // Offset 0x0D, Size 3
};

#pragma pack(1)
typedef struct
{
    UINT32 AttributeType;             // Offset 0x00, Size 4
    UINT32 Length;                    // Offset 0x04, Size 4
    UINT8  NonResidentFlag;           // Offset 0x08, Size 1
    UINT8  NameLength;                // Offset 0x09, Size 1
    UINT16 NameOffset;                // Offset 0x0A, Size 2
    UINT16 Flags;                     // Offset 0x0C, Size 2
    UINT16 AttributeID;               // Offset 0x0E, Size 2
} IAttribute, *PIAttribute;

#pragma pack(1)
struct ResidentAttribute : IAttribute
{
    /* Value of AttributeOffset should be (2 * NameLength + 0x18).
     * Size of AttributeName should be (2 * NameLength).
     * Attribute contents has an offset of (2 * NameLength + 0x18).
     */
    UINT32 AttributeLength;           // Offset 0x10, Size 4
    UINT16 AttributeOffset;           // Offset 0x14, Size 2
    UINT8  IndexedFlag;               // Offset 0x16, Size 1
    UINT8  Padding;                   // Offset 0x17, Size 1
};

#pragma pack(1)
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

/* Macro to get data pointer from a resident attribute pointer. */
#define GetResidentDataPointer(x) (char*)x + ((ResidentAttribute*)x)->AttributeOffset

/* Macro to free memory from data run. */
#define FreeDataRun(x) while(x) { PDataRun tmp = ((PDataRun)x)->NextRun; delete x; x = tmp; }

/* $STANDARD_INFORMATION (0x10) */
#pragma pack(1)
struct StandardInformationEx
{
    UINT64 FileCreation;          // Offset 0x00, Size 8
    UINT64 FileAltered;           // Offset 0x08, Size 8
    UINT64 MFTChanged;            // Offset 0x10, Size 8
    UINT64 FileRead;              // Offset 0x18, Size 8
    UINT32 FilePermissions;       // Offset 0x20, Size 4
    UINT32 MaxVersions;           // Offset 0x24, Size 4
    UINT32 VersionNum;            // Offset 0x28, Size 4
    UINT32 ClassId;               // Offset 0x2C, Size 4
    UINT32 OwnerId;               // Offset 0x30, Size 4
    UINT32 SecurityId;            // Offset 0x34, Size 4
    UINT64 QuotaCharged;          // Offset 0x38, Size 8
    UINT64 UpdateSequenceNumber;  // Offset 0x40, Size 8
};

/* *** EXTENDED ATTRIBUTE HEADERS *** */

/* $ATTRIBUTE_LIST (0x20) */
struct AttributeListEx
{
    UINT32 Type;          // Offset 0x00, Size 4
    UINT16 RecordLength;  // Offset 0x04, Size 2
    UINT8  NameLength;    // Offset 0x06, Size 1
    UINT8  NameOffset;    // Offset 0x07, Size 1
    UINT64 FirstVCN;      // Offset 0x08, Size 8
    UINT64 BaseFileRef;   // Offset 0x10, Size 2
    UINT16 AttributeId;   // Offset 0x18, Size 2
};

/* $FILE_NAME (0x30) */
#pragma pack(1)
struct FileNameEx
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

/* $OBJECT_ID (0x40) */
struct ObjectIdEx
{
    GUID ObjectId;    // Offset 0x00, Size 16
    GUID BirthVolId;  // Offset 0x10, Size 16
    GUID BirthObjId;  // Offset 0x20, Size 16
    GUID DomainId;    // Offset 0x30, Size 16
};

/* $SECURITY_DESCRIPTOR (0x50) */
/*struct SECURITY_DESCRIPTOR
{
    // TODO: Complete
};*/

/*$VOLUME_INFORMATION (0x70) */
struct VolumeInformationEx
{
    UINT64 Reserved1;     // Offset 0x00, Size 8
    UINT8  MajorVersion;  // Offset 0x08, Size 1
    UINT8  MinorVersion;  // Offset 0x09, Size 1
    UINT16 Flags;         // Offset 0x0A, Size 2
    UINT32 Reserved2;     // Offset 0x0C, Size 4
};

/* $INDEX_ROOT (0x90)  */
struct IndexRootEx
{
    UINT32 AttrType;             // Offset 0x00, Size 4
    UINT32 CollationRule;        // Offset 0x04, Size 4
    UINT32 BytesPerIndexRec;     // Offset 0x08, Size 4
    UINT8  ClusPerIndexRec;      // Offset 0x0C, Size 1
    IndexNodeHeader NodeHeader;  // Offset 0x10, Size 16
};

/* $REPARSE_POINT (0xC0) */
struct ReparsePointEx
{
    UINT32 ReparseType;        // Offset 0x00, Size 4
    UINT16 ReparseDataLength;  // Offset 0x04, Size 2
    UINT16 Padding;            // Offset 0x06, Size 2
};

struct ThirdPartyReparsePointEx : ReparsePointEx
{
    GUID ReparseGUID;          // Offset 0x08, Size 16
};

/* $EA_INFORMATION (0xD0) */
struct EAInformationEx
{
    UINT16 PackedEASize;      // Offset 0x00, Size 2
    UINT16 NumEAWithNEED_EA;  // Offset 0x02, Size 2
    UINT32 UnpackedEASize;    // Offset 0x04, Size 4
};

/* $EA (0xE0) */
struct EAEx
{
    UINT32 OffsetNextEA;  // Offset 0x00, Size 4
    UINT8  Flags;         // Offset 0x04, Size 1
    UINT8  NameLength;    // Offset 0x05, Size 1
    UINT16 ValueLength;   // Offset 0x06, Size 2
};