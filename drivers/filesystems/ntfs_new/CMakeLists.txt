
list(APPEND SOURCE
    mft.cpp
    ntfs.cpp
    ntfsdriver.cpp
    util.cpp
    fastio.cpp
    ntblockio.cpp
    ntfspartition.cpp
    ntfs.h)

add_library(ntfs MODULE ${SOURCE} ntfs.rc)
set_module_type(ntfs kernelmodedriver)
target_link_libraries(ntfs ${PSEH_LIB})
add_importlibs(ntfs ntoskrnl hal)
add_pch(ntfs ntfs.h SOURCE)
add_cd_file(TARGET ntfs DESTINATION reactos/system32/drivers FOR all)
