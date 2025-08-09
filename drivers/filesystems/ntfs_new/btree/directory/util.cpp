/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

BOOLEAN
Directory::IsLegalShortNameCharacterW(_In_ WCHAR Char)
{
    /* Legal characters include:
     * 0-9, A-Z, !, #, $, %, &, ', (, ), -, @, ^, _, `, {, }, ~
     */
    WCHAR AllowedCharacters[17] = L"!#$%&'()-@^_`{}~";

    if ((Char >= L'0' && Char <= L'9') || (Char >= L'A' && Char <= L'Z'))
        return TRUE;

    for (int i = 0; i < wcslen(AllowedCharacters); i++)
    {
        if (Char == AllowedCharacters[i])
            return TRUE;
    }

    return FALSE;
}

/* TODO:
 * Determine if this should be replaced with RtlIsNameLegalDOS8Dot3().
 *
 * RtlIsNameLegalDOS8Dot3() does not currently work for this purpose;
 * however our implementation may be incorrect. Write tests and experiment
 * on Windows.
 */
BOOLEAN
Directory::IsLegal8Dot3ShortName(_In_ PWSTR Buffer,
                                 _In_ USHORT Length)
{
    USHORT LengthBeforeDot, LengthAfterDot;
    BOOLEAN ReachedDot;

    // Fail if the length is too long.
    if (Length > MAX_SHORTNAME_LENGTH)
        return FALSE;

    // '.' and '..' are valid and special cases.
    if (IsDotOrDotDotDirW(Buffer, Length))
        return TRUE;

    ReachedDot = FALSE;
    LengthBeforeDot = 0;
    LengthAfterDot = 0;

    for (int i = 0; i < Length; i++)
    {
        // Handle dot character
        if (Buffer[i] == L'.')
        {
            if (ReachedDot)
            {
                return FALSE;
            }
            ReachedDot = TRUE;
            continue;
        }

        // Fail if this is an illegal character
        if (!(IsLegalShortNameCharacterW(Buffer[i]) ||
              IsLowercaseCharacterW(Buffer[i])))
        {
            return FALSE;
        }

        // Count this element
        if (!ReachedDot)
        {
            LengthBeforeDot++;
            if (LengthBeforeDot > 8)
            {
                return FALSE;
            }
        }

        else
        {
            LengthAfterDot++;
            if (LengthAfterDot > 3)
            {
                return FALSE;
            }
        }
    }

    // If a file name has an extension, it must at least be 1 character long.
    if (ReachedDot && !LengthAfterDot)
    {
        return FALSE;
    }

    // We passed all tests.
    return TRUE;
}
