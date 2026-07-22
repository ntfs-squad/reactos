/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing WakeConditionVariable on pre-NT6 builds
 */

#include "condvar_compat.h"

static
VOID
WINAPI
Compat_WakeConditionVariable(PCOMPAT_CONDITION_VARIABLE ConditionVariable)
{
    static VOID (NTAPI *pRtlWakeConditionVariable)(PCOMPAT_CONDITION_VARIABLE);

    if (!pRtlWakeConditionVariable)
    {
        pRtlWakeConditionVariable = (VOID (NTAPI *)(PCOMPAT_CONDITION_VARIABLE))
            Compat_GetNtdllProc("RtlWakeConditionVariable");
        if (!pRtlWakeConditionVariable)
            return;
    }
    pRtlWakeConditionVariable(ConditionVariable);
}

VOID (WINAPI *__imp_WakeConditionVariable)(PCOMPAT_CONDITION_VARIABLE)
    IMP_SYM("WakeConditionVariable", "4") = Compat_WakeConditionVariable;
