set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

# Handle absolute include and lib dirs outside of CMAKE_INSTALL_PREFIX
include(JoinPaths.cmake) # can be replaced by cmake_path(APPEND) in CMake 3.20
join_paths(PKGCONFIG_INCLUDEDIR "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
join_paths(PKGCONFIG_LIBDIR "\${prefix}" "${CMAKE_INSTALL_LIBDIR}")

# Build rules for MD4C parser library

configure_file(md4c.pc.in md4c.pc @ONLY)
add_library(md4c md4c.c md4c.h)
target_include_directories(md4c PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(md4c PRIVATE "$<$<CONFIG:Debug>:DEBUG>")
set_target_properties(md4c PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER md4c.h
)

# Build rules for HTML renderer library

configure_file(md4c-html.pc.in md4c-html.pc @ONLY)
add_library(md4c-html md4c-html.c md4c-html.h entity.c entity.h)
target_include_directories(md4c-html PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
set_target_properties(md4c-html PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER md4c-html.h
)
target_link_libraries(md4c-html PUBLIC md4c)


# Install rules

install(
    TARGETS md4c
    EXPORT md4cConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/md4c.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(
    TARGETS md4c-html
    EXPORT md4cConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/md4c-html.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(EXPORT md4cConfig DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/md4c/ NAMESPACE md4c::)
