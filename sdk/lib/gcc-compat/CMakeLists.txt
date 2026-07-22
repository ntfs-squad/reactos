
include_directories(
    ${REACTOS_SOURCE_DIR}/sdk/lib/crt/include)

list(APPEND SOURCE
    __mingw_vsprintf.c
    __throw_out_of_range_fmt.cpp
    StringCchCopyNA.c)

if(DLL_EXPORT_VERSION LESS 0x600)
    list(APPEND SOURCE
        rand_s.c
        # CRT functions missing from pre-NT6 msvcrt, needed by libstdc++/libmingwex
        btowc.c
        wctob.c
        wctype_compat.c
        ${REACTOS_SOURCE_DIR}/sdk/lib/crt/string/strnlen.c
        ${REACTOS_SOURCE_DIR}/sdk/lib/crt/string/wcsnlen.c
        # NT6 condition variable APIs referenced by libgcc's gthread layer (GCC >= 13)
        InitializeConditionVariable.c
        SleepConditionVariableCS.c
        WakeAllConditionVariable.c
        WakeConditionVariable.c)
endif()

add_library(stdc++compat ${SOURCE})
set_target_cpp_properties(stdc++compat WITH_EXCEPTIONS)
add_dependencies(stdc++compat xdk)
target_link_libraries(stdc++compat libmsvcrt)
remove_target_compile_option(stdc++compat "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<IN_LIST:cppstl,$<TARGET_PROPERTY:LINK_LIBRARIES>>>>:-nostdinc>")
target_compile_definitions(stdc++compat PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:PAL_STDCPP_COMPAT>" _CRT_DECLARE_NONSTDC_NAMES=1)
