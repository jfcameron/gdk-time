# © Joseph Cameron - All Rights Reserved

jfc_add_tests(
    C++_STANDARD 20
    C_STANDARD 90

    TEST_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/game_loop_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/fixed_step_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/rate_types_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/host_loop_test.cpp"

    INCLUDE_DIRECTORIES
        "${${PROJECT_NAME}_INCLUDE_DIRECTORIES}"

    LIBRARIES
        "${${PROJECT_NAME}_LIBRARIES}")

file(GLOB_RECURSE GDKTIME_PUBLIC_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/../include/gdk/*.h")

set(GDKTIME_SELF_CONTAINMENT_SOURCES "")

foreach (_header ${GDKTIME_PUBLIC_HEADERS})
    file(RELATIVE_PATH _relative "${CMAKE_CURRENT_LIST_DIR}/../include" "${_header}")

    string(MAKE_C_IDENTIFIER "${_relative}" _symbol)

    set(_generated "${CMAKE_CURRENT_BINARY_DIR}/self_containment/${_symbol}.cpp")

    file(WRITE "${_generated}" "#include <${_relative}>\n")

    list(APPEND GDKTIME_SELF_CONTAINMENT_SOURCES "${_generated}")
endforeach()

add_library(gdktime_header_self_containment OBJECT ${GDKTIME_SELF_CONTAINMENT_SOURCES})

target_include_directories(gdktime_header_self_containment
    PRIVATE "${gdktime_INCLUDE_DIRECTORIES}")

set_target_properties(gdktime_header_self_containment PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

if (MSVC)
    target_compile_options(gdktime_header_self_containment PRIVATE /permissive- /W4 /WX)
else()
    target_compile_options(gdktime_header_self_containment PRIVATE
        -pedantic-errors -Wall -Wextra -Werror)
endif()
