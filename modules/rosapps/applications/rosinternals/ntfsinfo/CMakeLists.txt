list(APPEND SOURCE
     ntfsinfo.cpp
     ntfsinfo.rc)
add_executable(ntfsinfo ${SOURCE})
set_module_type(ntfsinfo win32cui UNICODE)
target_link_libraries(ntfsinfo cppstl)
set_target_cpp_properties(ntfsinfo WITH_EXCEPTIONS)
add_importlibs(ntfsinfo msvcrt kernel32 ntdll)
add_cd_file(TARGET ntfsinfo DESTINATION reactos/system32 FOR all)
