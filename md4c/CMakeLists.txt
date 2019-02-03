
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_definitions(-DMD4C_USE_UTF8)

set(md4c_src
    md4c.c
)

add_library(md4c ${md4c_src})

set_target_properties(md4c PROPERTIES
    VERSION ${MD_VERSION}
    SOVERSION ${MD_VERSION_MAJOR}
    PUBLIC_HEADER md4c.h
)
