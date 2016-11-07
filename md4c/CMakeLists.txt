
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_library(md4c STATIC md4c.c md4c.h)
