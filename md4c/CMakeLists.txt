# Be sure to export all symbols in Windows.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

set(md4c_src
    md4c.c
)

add_library(md4c ${md4c_src})

set_target_properties(md4c PROPERTIES
    VERSION ${MD_VERSION}
    SOVERSION ${MD_VERSION_MAJOR}
    PUBLIC_HEADER md4c.h
)

install(
    TARGETS md4c
    EXPORT md4cConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Create a pkg-config file
configure_file(md4c.pc.in md4c.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/md4c/md4c.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# And a CMake file
install(EXPORT md4cConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/md4c/)
