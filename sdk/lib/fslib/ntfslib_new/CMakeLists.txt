add_library(ntfslib_new)
add_subdirectory(src)
add_dependencies(ntfslib_new psdk)

target_include_directories(ntfslib_new
    PUBLIC include/public
    PUBLIC include/internal # hack
)

add_subdirectory(env)
