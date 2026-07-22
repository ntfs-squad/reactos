/*
 * PROJECT:     GCC C++ support library
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Workaround for missing wctype() in msvcrt on pre-NT6 builds,
 *              referenced by libstdc++'s ctype<wchar_t> members
 */

#define _CRTIMP
#include <string.h>
#include <ctype.h>
#include <wctype.h>

wctype_t
__cdecl
wctype(const char *property)
{
    static const struct
    {
        const char name[8];
        wctype_t mask;
    } map[] =
    {
        { "alnum",  _ALPHA | _DIGIT },
        { "alpha",  _ALPHA },
        { "blank",  _BLANK },
        { "cntrl",  _CONTROL },
        { "digit",  _DIGIT },
        { "graph",  _PUNCT | _ALPHA | _DIGIT },
        { "lower",  _LOWER },
        { "print",  _BLANK | _PUNCT | _ALPHA | _DIGIT },
        { "punct",  _PUNCT },
        { "space",  _SPACE },
        { "upper",  _UPPER },
        { "xdigit", _HEX },
    };
    unsigned int i;

    if (property)
    {
        for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        {
            if (strcmp(property, map[i].name) == 0)
                return map[i].mask;
        }
    }
    return 0;
}
