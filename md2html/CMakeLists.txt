
# Build & install rules for md2html command line utility
if(BUILD_MD2HTML_EXECUTABLE)
    add_executable(md2html cmdline.c cmdline.h md2html.c)
    target_link_libraries(md2html PRIVATE md4c-html)
    target_compile_definitions(md2html PRIVATE
        MD_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        MD_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        MD_VERSION_RELEASE=${PROJECT_VERSION_PATCH}
    )

    # On macOS, the installed executable needs an rpath to locate the shared
    # libraries (installed into ${CMAKE_INSTALL_LIBDIR}) from the bin directory.
    # @loader_path resolves relative to the executable's location at runtime.
    if(APPLE AND BUILD_SHARED_LIBS)
        set_target_properties(md2html PROPERTIES
            INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
        )
    endif()

    install(
        TARGETS md2html
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES "md2html.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
endif()
