
FetchContent_Declare(
    nuklear
    GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear.git
    GIT_TAG master
    GIT_SHALLOW ON
)
FetchContent_GetProperties(nuklear)
if(NOT nuklear_POPULATED)
    FetchContent_Populate(nuklear)
    add_library(nuklear INTERFACE ${nuklear_SOURCE_DIR}/nuklear.h)
    target_compile_features(nuklear INTERFACE c_std_90)
    target_include_directories(nuklear INTERFACE ${nuklear_SOURCE_DIR})
    find_library(MATH_LIBRARY m)
    if(MATH_LIBRARY)
        target_link_libraries(nuklear INTERFACE ${MATH_LIBRARY})
        list(APPEND NKUI_PLAT_DEPS ${MATH_LIBRARY})
    endif()
endif()
