/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     NTFS filesystem driver
 * COPYRIGHT:   Copyright 2024 Carl Bialorucki <carl.bialorucki@reactos.org>
 *              Copyright 2024 Justin Miller <justin.miller@reactos.org>
 */

#include "ntfspch.h"

#define InvalidMftZoneReservation(Num) (Num < 0 || Num > 100)
#define InvalidMftZoneReservationHDD(Num) (Num < 0 || Num > 4)
#define InvalidDisableLastAccessUpdate(Num) (Num < 0 || Num > 3)
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define KEYVAL_BUFFER_SIZE ROUND_UP(sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG), 0x10)

BOOLEAN gAllowExtChar8dot3;
BOOLEAN gShowMetadataFiles;
BOOLEAN gShowVersionInfo;
BOOLEAN gBugCheckOnCorrupt;
BOOLEAN gDisable8dot3NameCreation;
INT gDisableLastAccessUpdate;
BOOLEAN gDisableLfsDowngrade;
BOOLEAN gDisableLfsUpgrade;
INT gMftZoneReservation;
INT gMftZoneReservationHdd;

HANDLE
OpenRegistryKey()
{
    NTSTATUS Status;
    HANDLE hRegistryKey;
    UNICODE_STRING RegistryPath;
    OBJECT_ATTRIBUTES Attributes;

    // Set up registry path
    RtlInitUnicodeString(&RegistryPath,
                         L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\FileSystem");

    InitializeObjectAttributes(&Attributes,
                               &RegistryPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&hRegistryKey, KEY_READ, &Attributes);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to open registry key!\n");
        return NULL;
    }

    // Caller must close this registry key.
    return hRegistryKey;
}

INT
QueryDwordRegistryValueEx(_In_ HANDLE RegistryKey,
                          _In_ PWCHAR Name,
                          _In_ INT Default)
{
    NTSTATUS Status;
    UNICODE_STRING ValueName;
    UCHAR Buffer[KEYVAL_BUFFER_SIZE];
    ULONG DataLength;

    if (!RegistryKey)
        return Default;

    // Set up registry value
    RtlInitUnicodeString(&ValueName, Name);

    Status = ZwQueryValueKey(RegistryKey,
                             &ValueName,
                             KeyValuePartialInformation,
                             Buffer,
                             KEYVAL_BUFFER_SIZE,
                             &DataLength);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to query registry value \"%S\"!\n", Name);
        return Default;
    }

    return *((INT*)(((PKEY_VALUE_PARTIAL_INFORMATION)Buffer)->Data));
}

INT
QueryDwordRegistryValue(_In_ PWCHAR Name,
                        _In_ INT Default)
{
    HANDLE RegistryKey;

    RegistryKey = OpenRegistryKey();
    if (!RegistryKey)
        return Default;
    return QueryDwordRegistryValueEx(RegistryKey, Name, Default);
}

BOOLEAN
QueryBooleanRegistryValueEx(_In_ HANDLE RegistryKey,
                            _In_ PWCHAR Name,
                            _In_ BOOLEAN Default)
{
    return !!QueryDwordRegistryValueEx(RegistryKey, Name, Default ? 1 : 0);
}

BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name,
                          _In_ BOOLEAN Default)
{
    return !!QueryDwordRegistryValue(Name, Default ? 1 : 0);
}

NTSTATUS
SetDwordRegistryValue(_In_ HANDLE RegistryKey,
                      _In_ PWCHAR Name,
                      _In_ INT Data)
{
    UNICODE_STRING ValueName;

    if (!RegistryKey)
        return STATUS_INVALID_PARAMETER;

    // Set up registry value
    RtlInitUnicodeString(&ValueName, Name);

    return ZwSetValueKey(RegistryKey,
                         &ValueName,
                         0,
                         REG_DWORD,
                         &Data,
                         sizeof(INT));
}

NTSTATUS
SetBooleanRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ BOOLEAN Data)
{
    return SetDwordRegistryValue(RegistryKey, Name, Data ? 1 : 0);
}

VOID
GetGlobalSettingsFromRegistry()
{
    HANDLE RegistryKey;

    RegistryKey = OpenRegistryKey();

    /* *** Inherited Options *** */

    /* Allows extended characters, including diacritics, for 8.3 compliant
     * file names. Default is OFF (0).
     */
    gAllowExtChar8dot3 = QueryBooleanRegistryValueEx(RegistryKey,
                                                     L"NtfsAllowExtendedCharacter8dot3Rename",
                                                     FALSE);

    /* Initiates a Bug Check when file corruption is detected, instead of
     * trying to repair it. Default is OFF (0).
     */
    gBugCheckOnCorrupt = QueryBooleanRegistryValueEx(RegistryKey,
                                                     L"NtfsBugCheckOnCorrupt",
                                                     FALSE);

    /* Disables generating an 8.3 compliant name when a file has a
     * non-compliant name. This feature has a significant performance impact,
     * but allows older Windows programs to access these files.
     * Default is OFF (0).
     */
    gDisable8dot3NameCreation = QueryBooleanRegistryValueEx(RegistryKey,
                                                            L"NtfsDisable8dot3NameCreation",
                                                            FALSE);

    /* Enables or disables the last access time on all files and all volumes.
     * Options:
     *     0 - User Managed, Enabled
     *     1 - User Managed, Disabled
     *     2 - System Managed, Enabled (default)
     *     3 - System Managed, Disabled
     */
    gDisableLastAccessUpdate = QueryDwordRegistryValueEx(RegistryKey,
                                                         L"NtfsDisableLastAccessUpdate",
                                                         2);

    if (InvalidDisableLastAccessUpdate(gDisableLastAccessUpdate))
    {
        // We don't care if this fails or not, just give it a try.
        SetDwordRegistryValue(RegistryKey, L"NtfsDisableLastAccessUpdate", 2);
        gMftZoneReservation = 2;
    }

    /* Disables LFS downgrade when a volume is cleanly unmounted. The LFS
     * downgrade is required for interoperability with older versions of
     * Windows. Default is OFF (0).
     *
     * Note: This option currently has no effect.
     */
    gDisableLfsDowngrade = QueryBooleanRegistryValueEx(RegistryKey,
                                                       L"NtfsDisableLfsDowngrade",
                                                       FALSE);

    /* Disables LFS upgrade when a volume is mounted. The LFS upgrade can cause
     * compatibility issues with older versions of Windows when the volume is
     * not cleanly shut down or dismounted. Default is OFF (0).
     *
     * Note: This option currently has no effect.
     */
    gDisableLfsUpgrade = QueryBooleanRegistryValueEx(RegistryKey,
                                                     L"NtfsDisableLfsUpgrade",
                                                     FALSE);

    /* Changes the amount of space reserved for the Master File Table (MFT)
     * by default.
     *
     * Options:
     *     For Windows 7 and earlier:
     *
     *     0 - 12.5% of the disk (same as 1)
     *     1 - 12.5% of the disk (default)
     *     2 - 25.0% of the disk
     *     3 - 37.5% of the disk
     *     4 - 50.0% of the disk
     *
     *     For Windows 8+:
     *     0                - 200 MB (default)
     *     1 - 100          - gMftZoneReservation * 200 MB
     *     All other values - 200 MB
     *
     *     MFT Zone size on various partition sizes:
     *     * Assuming 4096 bytes per cluster
     *     * MFT zone includes MFT when full size is available, 
     *     * else MFT starts before MFT zone with varying levels of overlap depending on how small the size is.
     *     * Note: Windows 8.1 fsutil says:
     *           "MftZone is a reserved region of the disk used to make the MFT contiguous. The
     *            200 MB multiplier is only applicable for volumes > 1.5 GB.  Below this threshold
     *            the multiplier is scaled based on volume size."
     *       (I think it's probably supposed to be 2GB)
     *
     *     8 MB (minimum): 0x100 clusters  (~ 1   MB)
     *     10 MB:          0x100 clusters  (~ 1   MB)
     *     50 MB:          0x660 clusters  (~ 6.4 MB)
     *     100 MB:         0xCA0 clusters  (~ 12  MB)
     *     500 MB:         0x3EA0 clusters (~ 63  MB)
     *     1 GB:           0x7D20 clusters (~ 125 MB)
     *     1.5 GB:         0xBB80 clusters (~ 187 MB or 12.5%), (~374 MB or 25%), (~562 MB or 37.5%), (~750 MB or 50%), (~937 MB or 62.5%), (~1000 MB or 66.7%), (~1000 MB), (~1000 MB)
     *     1.5 GiB:        0xBFC0 clusters
     *     slightly more than 1.5 GiB : 0xC060 clusters
     *     2 GB +:         0xC820 clusters (~ 200 MB)
     */
    gMftZoneReservation = QueryDwordRegistryValueEx(RegistryKey,
                                                    L"NtfsMftZoneReservation",
                                                    1);

    if (InvalidMftZoneReservation(gMftZoneReservation))
    {
        // We don't care if this fails or not, just give it a try.
        SetDwordRegistryValue(RegistryKey, L"NtfsMftZoneReservation", 2);
        gMftZoneReservation = 1;
    }

    /* *** ReactOS Extended Options *** */

    /* Shows the super hidden NTFS metadata files, including $MFT and $LogFile.
     * Default is OFF (0).
     */
    gShowMetadataFiles = QueryBooleanRegistryValueEx(RegistryKey,
                                                     L"NtfsShowMetadataFiles",
                                                     FALSE);

    /* Shows the NTFS version information, including the LFS Client version,
     * in the properties window for each NTFS volume. Default is OFF (0).
     */
    gShowVersionInfo = QueryBooleanRegistryValueEx(RegistryKey,
                                                   L"NtfsShowVersionInfo",
                                                   FALSE);

    CloseRegistryKey(RegistryKey);
}
