cmake_minimum_required(VERSION 3.12)

project(MD4C
    VERSION "0.5.3"
    HOMEPAGE_URL "https://github.com/mity/md4c"
    LANGUAGES C
)

option(BUILD_MD2HTML_EXECUTABLE "Whether to compile the md2html executable" ON)


if(WIN32)
    # On Windows, given there is no standard lib install dir etc., we rather
    # by default build static lib.
    option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
else()
    # On Linux, MD4C is slowly being adding into some distros which prefer
    # shared lib.
    option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
endif()

set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE})

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
endif()


if(CMAKE_C_COMPILER_ID MATCHES GNU|Clang)
    add_compile_options(-Wall -Wextra -Wshadow)

    # We enforce -Wdeclaration-after-statement because Qt project needs to
    # build MD4C with Integrity compiler which chokes whenever a declaration
    # is not at the beginning of a block.
    add_compile_options(-Wdeclaration-after-statement)
elseif(MSVC)
    # Disable warnings about the so-called unsecured functions:
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)

    # Specify proper C runtime library:
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_RELWITHDEBINFO "{$CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()

include(GNUInstallDirs)

add_subdirectory(src)
if (BUILD_MD2HTML_EXECUTABLE)
    add_subdirectory(md2html)
    set(MINIMUM_PYTHON_VERSION "3.6")
    set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND AND NOT Python3_VERSION VERSION_LESS ${MINIMUM_PYTHON_VERSION})
        enable_testing()
        file(GLOB TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test/*.txt")
        # get_target_property(PROGRAM md2html LOCATION)
        set(PROGRAM "${CMAKE_CURRENT_BINARY_DIR}/md2html/md2html")
        foreach(TEST_FILE ${TEST_FILES})
            get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
            add_test(NAME ${TEST_NAME}
                     COMMAND ${Python3_EXECUTABLE} "${TEST_DIR}/run-testsuite.py" -s ${TEST_FILE} -p ${PROGRAM})
        endforeach()
            add_test(NAME Pathological_inputs
                     COMMAND ${Python3_EXECUTABLE} "${TEST_DIR}/pathological-tests.py" -p ${PROGRAM})
     else()
        message(WARNING "Python not found or version is less than ${MINIMUM_PYTHON_VERSION}.")
    endif()
endif()

