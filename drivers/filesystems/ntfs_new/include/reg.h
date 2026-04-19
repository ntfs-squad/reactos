// io/registry.cpp
HANDLE
OpenRegistryKey();

#define CloseRegistryKey(Key) ZwClose(Key)

INT
QueryDwordRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ INT Default = 0);

INT
QueryDwordRegistryValue(_In_ PWCHAR Name,
                        _In_ INT Default = 0);

BOOLEAN
QueryBooleanRegistryValue(_In_ HANDLE RegistryKey,
                          _In_ PWCHAR Name,
                          _In_ BOOLEAN Default = FALSE);
BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name,
                          _In_ BOOLEAN Default = FALSE);

NTSTATUS
SetDwordRegistryValue(_In_ HANDLE RegistryKey,
                      _In_ PWCHAR Name,
                      _In_ INT Data);

NTSTATUS
SetBooleanRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ BOOLEAN Data);

VOID
GetGlobalSettingsFromRegistry();
