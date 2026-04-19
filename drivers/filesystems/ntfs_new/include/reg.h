// io/registry.cpp
EXTERN_C
HANDLE
OpenRegistryKey();

#define CloseRegistryKey(Key) ZwClose(Key)

EXTERN_C
INT
QueryDwordRegistryValueEx(_In_ HANDLE RegistryKey,
                          _In_ PWCHAR Name,
                          _In_ INT Default);

EXTERN_C
INT
QueryDwordRegistryValue(_In_ PWCHAR Name,
                        _In_ INT Default);

EXTERN_C
BOOLEAN
QueryBooleanRegistryValueEx(_In_ HANDLE RegistryKey,
                            _In_ PWCHAR Name,
                            _In_ BOOLEAN Default);
EXTERN_C
BOOLEAN
QueryBooleanRegistryValue(_In_ PWCHAR Name,
                          _In_ BOOLEAN Default);

EXTERN_C
NTSTATUS
SetDwordRegistryValue(_In_ HANDLE RegistryKey,
                      _In_ PWCHAR Name,
                      _In_ INT Data);

EXTERN_C
NTSTATUS
SetBooleanRegistryValue(_In_ HANDLE RegistryKey,
                        _In_ PWCHAR Name,
                        _In_ BOOLEAN Data);

EXTERN_C
VOID
GetGlobalSettingsFromRegistry();
