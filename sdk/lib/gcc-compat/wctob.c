/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing wctob() in msvcrt on pre-NT6 builds,
 *              referenced by libstdc++'s ctype<wchar_t> members
 */

#define _CRTIMP
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <wchar.h>

int
__cdecl
wctob(wint_t wc)
{
    char buf[MB_LEN_MAX];

    if (wc == WEOF)
        return EOF;

    if (wctomb(buf, wc) == 1)
        return (unsigned char)buf[0];

    return EOF;
}
