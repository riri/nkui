target_sources(nkui PUBLIC nkui_xlib.h)
target_compile_definitions(nkui PUBLIC NKUI_BACKEND=NKUI_XLIB)

find_package(X11 REQUIRED)
set(NKUI_XLIB_DEPS X11::X11 X11::Xft)
list(APPEND NKUI_PLAT_DEPS ${NKUI_XLIB_DEPS})
target_link_libraries(nkui PUBLIC ${NKUI_PLAT_DEPS})
