
include_directories("${PROJECT_SOURCE_DIR}/md4c")

add_executable(md2html cmdline.c cmdline.h entity.c entity.h md2html.c render_html.c render_html.h)
target_link_libraries(md2html md4c)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/md2html DESTINATION ${CMAKE_INSTALL_BINDIR})
