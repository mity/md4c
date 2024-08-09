# Build rules for md2html command line utility

add_executable(md2html cmdline.c cmdline.h md2html.c)
target_link_libraries(md2html PRIVATE md4c-html)
target_compile_definitions(md2html PRIVATE
    MD_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    MD_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    MD_VERSION_RELEASE=${PROJECT_VERSION_PATCH}
)


# Install rules

install(
    TARGETS md2html
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES "md2html.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
