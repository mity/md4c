cmake_minimum_required(VERSION 3.14)

# ESP-IDF build system does not support CMake's project() command, 
# so we need to check if we're building as part of an ESP-IDF 
# project or standalone.
if(IDF_TARGET)
    idf_component_register(
        SRC_DIRS "src"
        INCLUDE_DIRS "src"
    )
endif()

if(NOT IDF_TARGET)
    project(MD4C
        VERSION "0.5.3"
        HOMEPAGE_URL "https://github.com/mity/md4c"
        LANGUAGES C
    )

    # Some embedded platforms need the following files to exist even before
    # CMake's configure time. Hence we have them directly in the repo, but lets
    # at least check, they're up to date.
    #
    # If not, after a review of the generated one in the build dir need to be
    # committed as replacement of the respective pre-generated one. Unless we
    # do some really significant change like directory restructuring or introduce
    # some lib dependencies, it should be just about keeping the version in sync
    # between these files.
    configure_file(idf_component.yml.in    idf_component.yml.expected    @ONLY)
    configure_file(library.json.in         library.json.expected         @ONLY)
    configure_file(library.properties.in   library.properties.expected   @ONLY)

    function(CheckPregeneratedFile rel_path)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol
            ${CMAKE_CURRENT_SOURCE_DIR}/${rel_path}
            ${CMAKE_CURRENT_BINARY_DIR}/${rel_path}.expected
            RESULT_VARIABLE compare_result
        )
        if(NOT compare_result EQUAL 0)
            message(FATAL_ERROR "The pre-generated ${rel_path} file is not up to date.")
        endif()
    endfunction()

    CheckPregeneratedFile(idf_component.yml)
    CheckPregeneratedFile(library.json)
    CheckPregeneratedFile(library.properties)

    # Some heuristics for what targets (and how) we actually build:
    set(BUILD_SHARED_LIBS_default ON)
    set(BUILD_HTML_LIBRARY_default ON)
    set(BUILDMD2HTML_EXECUTABLE_default ON)
    if(WIN32)
        # On Windows, given there is no standard lib install dir etc., we
        # rather by default build static lib.
        set(BUILD_SHARED_LIBS_default OFF)
    endif()
    if(NOT PROJECT_IS_TOP_LEVEL)
        # When we're embedded into a bigger project, most likely the outer
        # project only needs the libs.
        set(BUILDMD2HTML_EXECUTABLE_default OFF)
    endif()

    # Allow the user to override the defaults:
    option(
        BUILD_SHARED_LIBS
        "Build using shared libraries"
        ${BUILD_SHARED_LIBS_default}
    )
    option(
        BUILD_HTML_LIBRARY
        "Whether to compile the libmd-html library"
        ${BUILD_HTML_LIBRARY_default}
    )
    option(
        BUILD_MD2HTML_EXECUTABLE
        "Whether to compile the md2html command line tool (requires libmd-html)"
        ${BUILDMD2HTML_EXECUTABLE_default}
    )

    if(BUILD_MD2HTML_EXECUTABLE AND NOT BUILD_HTML_LIBRARY)
        message(FATAL_ERROR "BUILD_MD2HTML_EXECUTABLE requires BUILD_HTML_LIBRARY=ON")
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
    add_subdirectory(md2html)
endif()
