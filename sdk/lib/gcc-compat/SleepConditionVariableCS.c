/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing SleepConditionVariableCS on pre-NT6 builds
 */

#include "condvar_compat.h"

#define COMPAT_STATUS_TIMEOUT ((LONG)0x00000102)

static
BOOL
WINAPI
Compat_SleepConditionVariableCS(PCOMPAT_CONDITION_VARIABLE ConditionVariable, PVOID CriticalSection, DWORD Timeout)
{
    static LONG (NTAPI *pRtlSleepConditionVariableCS)(PCOMPAT_CONDITION_VARIABLE, PVOID, PLARGE_INTEGER);
    static ULONG (NTAPI *pRtlNtStatusToDosError)(LONG);
    LONG Status;
    LARGE_INTEGER Time, *TimePtr;

    if (!pRtlSleepConditionVariableCS)
    {
        pRtlSleepConditionVariableCS = (LONG (NTAPI *)(PCOMPAT_CONDITION_VARIABLE, PVOID, PLARGE_INTEGER))
            Compat_GetNtdllProc("RtlSleepConditionVariableCS");
        pRtlNtStatusToDosError = (ULONG (NTAPI *)(LONG))
            Compat_GetNtdllProc("RtlNtStatusToDosError");
        if (!pRtlSleepConditionVariableCS)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }
    }

    /* Same conversion and error mapping as kernel32_vista's SleepConditionVariableCS */
    if (Timeout == INFINITE)
    {
        TimePtr = NULL;
    }
    else
    {
        Time.QuadPart = (ULONGLONG)Timeout * -10000;
        TimePtr = &Time;
    }

    Status = pRtlSleepConditionVariableCS(ConditionVariable, CriticalSection, TimePtr);
    if (Status < 0 || Status == COMPAT_STATUS_TIMEOUT)
    {
        SetLastError(pRtlNtStatusToDosError ? pRtlNtStatusToDosError(Status) : (DWORD)Status);
        return FALSE;
    }
    return TRUE;
}

BOOL (WINAPI *__imp_SleepConditionVariableCS)(PCOMPAT_CONDITION_VARIABLE, PVOID, DWORD)
    IMP_SYM("SleepConditionVariableCS", "12") = Compat_SleepConditionVariableCS;
