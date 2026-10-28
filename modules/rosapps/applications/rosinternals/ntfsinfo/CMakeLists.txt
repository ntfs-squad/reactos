list(APPEND SOURCE
     ntfsinfo.c
     ntfsinfo.rc)
add_executable(ntfsinfo ${SOURCE})
set_module_type(ntfsinfo win32cui UNICODE)
target_link_libraries(ntfsinfo ntfslib_new ntfslib_new_um)
add_importlibs(ntfsinfo msvcrt kernel32 ntdll)
add_cd_file(TARGET ntfsinfo DESTINATION reactos/system32 FOR all)
