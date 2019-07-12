
include_directories("${PROJECT_SOURCE_DIR}/md4c")

add_executable(md2html cmdline.c cmdline.h entity.c entity.h md2html.c render_html.c render_html.h)
target_link_libraries(md2html md4c)

install(
    TARGETS md2html
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES "md2html.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
