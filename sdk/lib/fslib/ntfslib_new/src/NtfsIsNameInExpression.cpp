#include <ntfslib_new.h>
#include <ntfslib_new_internal.h>

BOOLEAN
NtfsIsNameInExpressionFallback(_In_     PUNICODE_STRING Expression,
                               _In_     PUNICODE_STRING Name,
                               _In_     BOOLEAN IgnoreCase,
                               _In_opt_ PWCHAR UpcaseTable)
{
    /* If this was just a kernel driver, we could call FsRtlIsNameInExpression() directly.
     * Windows 7 and later have RtlIsNameInExpression() in ntdll, but this needs to work
     * on ReactOS when compiled as NT 5.x and we may want this in FreeLdr as well.
     */

    // HACK: NOT IMPLEMENTED!
    return FALSE;
}
