
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
configure_file(md4c.pc.in md4c.pc @ONLY)


# Build rules for MD4C parser library

add_library(md4c md4c.c md4c.h)
set_target_properties(md4c PROPERTIES
    COMPILE_FLAGS "-DMD4C_USE_UTF8"
    VERSION ${MD_VERSION}
    SOVERSION ${MD_VERSION_MAJOR}
    PUBLIC_HEADER md4c.h
)


# Build rules for md2html command line utility

add_executable(md2html cmdline.c cmdline.h entity.c entity.h md2html.c render_html.c render_html.h)
target_link_libraries(md2html md4c)


# Install rules

install(
    TARGETS md4c
    EXPORT md4cConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    TARGETS md2html
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT md4cConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/md4c/)
install(FILES ${CMAKE_BINARY_DIR}/src/md4c.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES "md2html.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

