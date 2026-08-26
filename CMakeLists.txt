# © Joseph Cameron - All Rights Reserved

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/jfc-cmake/jfclib.cmake")

project(null)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GDKTIME_IS_TOP_LEVEL ON)
else()
    set(GDKTIME_IS_TOP_LEVEL OFF)
endif()

option(GDKTIME_BUILD_TESTS "Build unit tests" ${GDKTIME_IS_TOP_LEVEL})
option(GDKTIME_BUILD_DOCS "Build documentation" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
    endif()

    include(CheckCXXCompilerFlag)

    foreach (_strict_flag "-Werror=c++20-extensions" "-Werror=c++23-extensions")
        string(MAKE_C_IDENTIFIER "HAS${_strict_flag}" _strict_flag_symbol)

        check_cxx_compiler_flag("${_strict_flag}" ${_strict_flag_symbol})

        if (${_strict_flag_symbol})
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${_strict_flag}>)
        endif()
    endforeach()
endif()

jfc_project(library
    NAME "gdktime"
    VERSION 0.1
    DESCRIPTION "game development kit timing lib"
    C++_STANDARD 20
    C_STANDARD 90
    TYPE STATIC

    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/include

    SOURCE_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/src/game_loop.cpp
)

if (GDKTIME_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (GDKTIME_BUILD_DOCS)
    add_subdirectory(docs)
endif()
