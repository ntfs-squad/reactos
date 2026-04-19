list(APPEND SOURCE
     ntfspch.h
     io/close.cpp
     io/create.cpp
     io/ea.cpp
     io/fastio.cpp
     io/fileinfo.cpp
     io/fsctrl.cpp
     io/ntblockio.cpp
     io/ntfsinit.cpp
     io/pnp.cpp
     io/read.cpp
     io/registry.cpp
     io/vol.cpp
     io/write.cpp
     ntfsvol/disk.cpp
     ntfsvol/ntfsvol.cpp
     ntfsvol/sanity.cpp
     )

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB} memcmp ntfslib_new)
if(GDB AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ntfs PRIVATE -O0)
endif()
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs ntfspch.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers NO_CAB FOR all)
