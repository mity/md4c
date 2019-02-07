
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_definitions(-DMD4C_USE_UTF8)

set(md4c_src
    md4c.c
)

add_library(md4c SHARED ${md4c_src})

set_target_properties(md4c PROPERTIES
    VERSION ${MD_VERSION}
    SOVERSION ${MD_VERSION_MAJOR}
    PUBLIC_HEADER md4c.h
)

install(
    TARGETS md4c
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Create a pkg-config file
configure_file(md4c.pc.in md4c.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/md4c/md4c.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
