list(APPEND SOURCE
     util.cpp)

add_library(ntfslib_new ${SOURCE})
add_dependencies(ntfslib_new psdk)
