/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing InitializeConditionVariable on pre-NT6 builds
 */

#include "condvar_compat.h"

static
VOID
WINAPI
Compat_InitializeConditionVariable(PCOMPAT_CONDITION_VARIABLE ConditionVariable)
{
    static VOID (NTAPI *pRtlInitializeConditionVariable)(PCOMPAT_CONDITION_VARIABLE);

    if (!pRtlInitializeConditionVariable)
    {
        pRtlInitializeConditionVariable = (VOID (NTAPI *)(PCOMPAT_CONDITION_VARIABLE))
            Compat_GetNtdllProc("RtlInitializeConditionVariable");
        if (!pRtlInitializeConditionVariable)
            return;
    }
    pRtlInitializeConditionVariable(ConditionVariable);
}

VOID (WINAPI *__imp_InitializeConditionVariable)(PCOMPAT_CONDITION_VARIABLE)
    IMP_SYM("InitializeConditionVariable", "4") = Compat_InitializeConditionVariable;
