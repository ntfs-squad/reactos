list(APPEND SOURCE
     io/ntfsinit.cpp
     io/util.cpp
     io/fastio.cpp
     io/read.cpp
     io/ea.cpp
     io/write.cpp
     io/vol/vol.cpp
     io/pnp.cpp
     io/create.cpp
     io/close.cpp
     io/fileinfo/fileinfo.cpp
     io/ntblockio.cpp
     io/fsctrl.cpp
     mft.cpp
     ntfspartition.cpp
     io/ntfsprocs.h)

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB} memcmp)
if(GDB AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ntfs PRIVATE -O0)
endif()
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs io/ntfsprocs.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers NO_CAB FOR all)
