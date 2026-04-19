list(APPEND SOURCE
     btree/directory/create.cpp
     btree/directory/dbg.cpp
     btree/directory/directory.cpp
     btree/directory/editdir.cpp
     btree/directory/find.cpp
     btree/directory/get.cpp
     btree/directory/util.cpp
     btree/btree.cpp
     filerecord/attributes/addattr.cpp
     filerecord/attributes/findattr.cpp
     filerecord/read/copy.cpp
     filerecord/write/filedata.cpp
     filerecord/filerecord.cpp
     filerecord/fixup.cpp
     lfs/lfs.cpp
     mft/get.cpp
     mft/mft.cpp
     # ntfsvol/disk.cpp
     ntfsvol/metadata/attrdef.cpp
     ntfsvol/metadata/bitmap.cpp
     ntfsvol/metadata/upcase.cpp
     ntfsvol/metadata/volume.cpp
     # ntfsvol/ntfsvol.cpp
     # ntfsvol/sanity.cpp
     util.cpp)

add_library(ntfslib_new ${SOURCE})
add_dependencies(ntfslib_new psdk)
