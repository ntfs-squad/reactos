list(APPEND SOURCE
     ntfsinit.cpp
     util.cpp
     fastio.cpp
     read.cpp
     ea.cpp
     write.cpp
     vol.cpp
     pnp.cpp
     create.cpp
     close.cpp
     fileinfo.cpp
     ntblockio.cpp
     fsctrl.cpp
     ntfslogic/mft.cpp
     ntfslogic/ntfspartition.cpp
     ntfsprocs.h)

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB} memcmp)
if(GDB AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ntfs PRIVATE -O0)
endif()
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs ntfsprocs.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers NO_CAB FOR all)
