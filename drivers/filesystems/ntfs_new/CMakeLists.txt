list(APPEND SOURCE
     close.c
     create.cpp
     ea.c
     fastio.c
     fileinfo.c
     fsctrl.c
     ntblockio.c
     ntfsinit.c
     pnp.c
     read.c
     registry.c
     vol.cpp
     write.cpp
     ntfsvol.cpp)

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB} memcmp ntfslib_new ntfslib_new_km)
if(GDB AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ntfs PRIVATE -O0)
endif()
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs ntfspch.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers NO_CAB FOR all)
