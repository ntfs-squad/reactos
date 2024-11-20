list(APPEND SOURCE
     ntfspch.h
     io/ntfsinit.cpp
     io/util.cpp
     io/fastio.cpp
     io/read.cpp
     io/ea.cpp
     io/write.cpp
     io/vol.cpp
     io/pnp.cpp
     io/create.cpp
     io/close.cpp
     io/fileinfo.cpp
     io/ntblockio.cpp
     io/fsctrl.cpp
     io/registry.cpp
     filerecord/attributes/addattr.cpp
     filerecord/attributes/findattr.cpp
     filerecord/read/copy.cpp
     filerecord/write/filedata.cpp
     filerecord/filerecord.cpp
     filerecord/fixup.cpp
     lfs/lfs.cpp
     ntfsvol/disk.cpp
     ntfsvol/ntfsvol.cpp
     btree/btree.cpp
     btree/directory/directory.cpp
     btree/directory/dbg.cpp
     btree/directory/get.cpp
     btree/directory/find.cpp
     mft/get.cpp
     mft/mft.cpp)

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB} memcmp)
if(GDB AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ntfs PRIVATE -O0)
endif()
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs ntfspch.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers NO_CAB FOR all)
