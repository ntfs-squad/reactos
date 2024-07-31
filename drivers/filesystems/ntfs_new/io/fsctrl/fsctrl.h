#include "../ntfsprocs.h"
#define NDEBUG
#include <debug.h>

static
BOOLEAN
NtfsIsIrpTopLevel (_In_ PIRP Irp)
{
    PAGED_CODE();

    if (!IoGetTopLevelIrp())
    {
        IoSetTopLevelIrp(Irp);
        return TRUE;
    }

    return FALSE;
}