/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for libgcc referencing NT6 condition variable APIs
 *              on pre-NT6 builds (gthr-win32-cond.o in GCC >= 13)
 */

#pragma once

#include <windef.h>
#include <winbase.h>

typedef struct _COMPAT_CONDITION_VARIABLE
{
    PVOID Ptr;
} COMPAT_CONDITION_VARIABLE, *PCOMPAT_CONDITION_VARIABLE;

/* The Rtl condition variable implementation is statically linked into our
 * ntdll (rtl_vista) and initialized by LdrpInitializeProcess, so it is
 * exported from ntdll.dll even on pre-NT6 builds. Resolve it at runtime:
 * not every module links the ntdll import library, but ntdll.dll is loaded
 * in every process, and anything pulling in gthr-win32-cond.o links
 * kernel32 anyway (for the CRITICAL_SECTION APIs). */
static __inline
FARPROC
Compat_GetNtdllProc(const char *Name)
{
    HMODULE Ntdll = GetModuleHandleW(L"ntdll.dll");
    return Ntdll ? GetProcAddress(Ntdll, Name) : NULL;
}

/* libgcc references these through their dllimport pointers only, so each
 * shim provides the __imp_ symbol (see rand_s.c for the same trick).
 * One symbol per file to avoid clashes with real import libraries. */
#ifdef _M_IX86
#define IMP_SYM(name, argsize) __asm__("__imp__" name "@" argsize)
#else
#define IMP_SYM(name, argsize) __asm__("__imp_" name)
#endif
