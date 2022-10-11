target_sources(nkui PUBLIC nkui_xlib.h)
target_compile_definitions(nkui PUBLIC NKUI_BACKEND=NKUI_XLIB)

find_package(X11 REQUIRED)
target_link_libraries(nkui PUBLIC X11::X11 X11::Xft)
