cmake_minimum_required(VERSION 3.21)
include(CMakeDependentOption)
include(FetchContent)

project(nkui
    VERSION 0.1.0
    DESCRIPTION "Nuklear integration made easy"
    LANGUAGES C
)

## manage build options

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # on top project, lets build everything
    set(NKUI_DEFAULT_STATIC ON CACHE INTERNAL "")
else()
    set(NKUI_DEFAULT_STATIC OFF CACHE INTERNAL "")
endif()

option(NKUI_BUILD_STATIC "Build a static library instead of an interface target" ${NKUI_DEFAULT_STATIC})
cmake_dependent_option(NKUI_OPENGL "Use OpenGL for drawing" OFF "NKUI_BUILD_STATIC" "")
cmake_dependent_option(NKUI_NO_NK_IMPL "Do not implement Nuklear" OFF "NKUI_BUILD_STATIC" "")
if(NKUI_BUILD_STATIC)
    set(NKUI_BACKEND "" CACHE STRING "Set backend for a static build (xlib, gdip, sdl, glfw)")
endif()

include(cmake/nuklear.cmake)

if(NKUI_BUILD_STATIC)
    set(NKUI_PRIVATE PRIVATE)
    set(NKUI_PUBLIC PUBLIC)
    add_library(nkui STATIC nkui.h nkui.c)
    if(NKUI_NO_NK_IMPL)
        target_compile_definitions(nkui PRIVATE NKUI_NO_NK_IMPLEMENTATION)
    endif()
    # add backend specific rules
    if(NKUI_BACKEND STREQUAL "")
        message(FATAL_ERROR "No NKUI_BACKEND defined")
    endif()
    if(NKUI_BACKEND STREQUAL "xlib")
        include(cmake/nkui_xlib.cmake)
    endif()
else()
    set(NKUI_PRIVATE INTERFACE)
    set(NKUI_PUBLIC INTERFACE)
    add_library(nkui INTERFACE nkui.h)
endif()

set(NKUI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(nkui ${NKUI_PUBLIC} "${NKUI_INCLUDE_DIR}")
target_link_libraries(nkui ${NKUI_PUBLIC} nuklear)
target_compile_features(nkui ${NKUI_PRIVATE} c_std_90)
if(MSVC)
    target_compile_options(nkui ${NKUI_PRIVATE} /W4 /WX)
else()
    target_compile_options(nkui ${NKUI_PRIVATE} -Wall -Wextra -pedantic)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_C_EXTENSIONS OFF)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    add_subdirectory(examples)
endif()
