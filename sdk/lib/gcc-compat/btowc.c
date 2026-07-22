/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing btowc() in msvcrt on pre-NT6 builds,
 *              referenced by libstdc++'s ctype<wchar_t> members
 */

#define _CRTIMP
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

wint_t
__cdecl
btowc(int c)
{
    wchar_t wc;
    char ch;

    if (c == EOF)
        return WEOF;

    ch = (char)(unsigned char)c;
    if (ch == '\0')
        return L'\0';

    if (mbtowc(&wc, &ch, 1) == 1)
        return wc;

    return WEOF;
}
