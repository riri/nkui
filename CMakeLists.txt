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
cmake_dependent_option(NKUI_OPENGL "Use OpenGL for drawing" OFF "NKUI_BUILD_STATIC" OFF)
cmake_dependent_option(NKUI_NO_NK_IMPL "Do not implement Nuklear" OFF "NKUI_BUILD_STATIC" OFF)
if(NKUI_BUILD_STATIC)
    set(NKUI_BACKEND "" CACHE STRING "Set backend for a static build (xlib, gdip, sdl, glfw)")
endif()
option(NKUI_IMAGE_LOADER "Include the image loading functions" ON)
option(NKUI_INCLUDE_STBIMAGE "Use self contained stb_image" ON)
cmake_dependent_option(NKUI_NO_STBIMAGE_IMPL "Do not implement stb_image" OFF "NKUI_BUILD_STATIC" OFF)

include(cmake/nuklear.cmake)

if(NKUI_BUILD_STATIC)
    if(NKUI_IMAGE_LOADER)
        list(APPEND NKUI_OPTIONS NKUI_IMAGE_LOADER)
    endif()
    if(NKUI_NO_NK_IMPL)
        list(APPEND NKUI_PRIV_OPTIONS NKUI_NO_NK_IMPLEMENTATION)
    endif()
    if(NKUI_INCLUDE_STBIMAGE)
        list(APPEND NKUI_PRIV_OPTIONS NKUI_INCLUDE_STBIMAGE)
    endif()
    if(NKUI_NO_STBIMAGE_IMPL)
        list(APPEND NKUI_PRIV_OPTIONS NKUI_NO_STBIMAGE_IMPL)
    endif()
    set(NKUI_PRIVATE PRIVATE)
    set(NKUI_PUBLIC PUBLIC)
    add_library(nkui STATIC nkui.h nkui.c)
    target_compile_definitions(nkui PRIVATE ${NKUI_PRIV_OPTIONS})
    target_compile_definitions(nkui PUBLIC ${NKUI_OPTIONS})

    # add backend specific rules
    if(NKUI_BACKEND STREQUAL "")
        message(FATAL_ERROR "No NKUI_BACKEND defined")
    endif()
    if(NKUI_BACKEND STREQUAL "xlib")
        include(cmake/nkui_xlib.cmake)
    endif()
else()
    if(NKUI_IMAGE_LOADER)
        list(APPEND NKUI_OPTIONS NKUI_IMAGE_LOADER)
    endif()
    set(NKUI_PRIVATE INTERFACE)
    set(NKUI_PUBLIC INTERFACE)
    add_library(nkui INTERFACE nkui.h)
    target_compile_definitions(nkui INTERFACE ${NKUI_OPTIONS})
endif()

set(NKUI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(nkui ${NKUI_PUBLIC} "${NKUI_INCLUDE_DIR}")
target_link_libraries(nkui ${NKUI_PUBLIC} nuklear)
target_compile_features(nkui ${NKUI_PRIVATE} c_std_90)
if(MSVC)
    target_compile_options(nkui ${NKUI_PRIVATE} /W4 /WX)
else()
    target_compile_options(nkui ${NKUI_PRIVATE} -Wall -Wextra -pedantic)
    if(NKUI_INCLUDE_STBIMAGE)
        # should check bad code and propose a PR to nothings/stb
        target_compile_options(nkui ${NKUI_PUBLIC} -Wno-misleading-indentation -Wno-shift-negative-value)
    endif()
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_C_EXTENSIONS OFF)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    add_subdirectory(examples)
endif()
