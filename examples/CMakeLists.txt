add_executable(simpliest simpliest.c)
set_target_properties(simpliest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(simple simple.c common.h common.c)
set_target_properties(simple PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(NKUI_BUILD_STATIC)
    target_link_libraries(simpliest PRIVATE nkui)
    target_link_libraries(simple PRIVATE nkui)
else()
    target_compile_definitions(simpliest PRIVATE NK_IMPLEMENTATION)
    target_compile_definitions(simple PRIVATE NK_IMPLEMENTATION)
endif()
