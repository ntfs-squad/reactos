/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing WakeAllConditionVariable on pre-NT6 builds
 */

#include "condvar_compat.h"

static
VOID
WINAPI
Compat_WakeAllConditionVariable(PCOMPAT_CONDITION_VARIABLE ConditionVariable)
{
    static VOID (NTAPI *pRtlWakeAllConditionVariable)(PCOMPAT_CONDITION_VARIABLE);

    if (!pRtlWakeAllConditionVariable)
    {
        pRtlWakeAllConditionVariable = (VOID (NTAPI *)(PCOMPAT_CONDITION_VARIABLE))
            Compat_GetNtdllProc("RtlWakeAllConditionVariable");
        if (!pRtlWakeAllConditionVariable)
            return;
    }
    pRtlWakeAllConditionVariable(ConditionVariable);
}

VOID (WINAPI *__imp_WakeAllConditionVariable)(PCOMPAT_CONDITION_VARIABLE)
    IMP_SYM("WakeAllConditionVariable", "4") = Compat_WakeAllConditionVariable;
