list(APPEND SOURCE
     io/ntfsinit.cpp
     io/ntfsprocs.h
     io/util.cpp
     io/fastio.cpp
     io/read/read.cpp
     io/ea.cpp
     io/write.cpp
     io/vol/vol.cpp
     io/pnp.cpp
     io/create/create.cpp
     io/close.cpp
     io/fileinfo/fileinfo.cpp
     io/ntblockio.cpp
     io/fsctrl/fsctrl.cpp
     filerecord/copy.cpp
     filerecord/filerecord.cpp
     filerecord/find.cpp
     filerecord/write.cpp
     ntfsvol.cpp
     btree/btree.cpp)

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB} memcmp)
if(GDB AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ntfs PRIVATE -O0)
endif()
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs io/ntfsprocs.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers NO_CAB FOR all)
