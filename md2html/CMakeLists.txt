
include_directories("${PROJECT_SOURCE_DIR}/md4c")

add_executable(md2html cmdline.c cmdline.h md2html.c)
target_link_libraries(md2html md4c)
