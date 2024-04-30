#include <ntifs.h>

class FileContextBlock
{
    PFILE_OBJECT FileObject;

    ULONGLONG MFTIndex;
};
