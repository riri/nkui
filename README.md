# Nuklear integration made easy

Provides a consistent way to us nuklear with unified backends.

## Simpliest nuklear program ever

```c
#include <stdio.h>
#define NKUI_IMPLEMENTATION
#include <nkui.h>

void draw_ui(struct nk_context *ctx, int width, int height, void *userdata) {
    enum { EASY, HARD };
    static int option = HARD;
    static float value = 0.8f;

    NK_UNUSED(userdata);

    if (nk_begin(ctx, "", nk_rect(0, 0, width, height), 0)) {
        nk_layout_row_static(ctx, 30, 80, 1);
        if (nk_button_label(ctx, "button")) {
            puts("button pressed");
        }

        /* fixed widget window ratio width */
        nk_layout_row_dynamic(ctx, 30, 2);
        if (nk_option_label(ctx, "easy", option == EASY)) option = EASY;
        if (nk_option_label(ctx, "hard", option == HARD)) option = HARD;

        /* custom widget pixel width */
        nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
            nk_layout_row_push(ctx, 50);
            nk_label(ctx, "Volume:", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 110);
            nk_slider_float(ctx, 0, &value, 1.0f, 0.1f);
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

int main() { return nkui_run(draw_ui, 0); }
```

To build it, just link to the needed libraries depending on your platform (or use the CMake target).
For example on Linux (X11/Xft):
```sh
CFLAGS="-O2 -DNKUI_BACKEND=NKUI_NATIVE `pkg-config --cflags xft`"
LDFLAGS="-lm -lX11 `pkg-config --libs xft`"
cc -o simpliest -Ipath/to/nkui -Ipath/to/nuklear simpliest.c
```

Nkui follows the full pedantic C that Nuklear aims to respect, so you can safely add to your compile options (gcc):
```
-Wall -Wextra -Werror -pedantic -std=c99
```

## There is more

Of course, that's the shortest way, for quick and dirty tests. Nkui in fact provides a cross-platform API to easily integrate nuklear in a project. `nkui_run()` is a shortcut function equivalent to this:

```c
struct nkui_params params;
NK_MEMCPY(&params, &nkui_default_params, sizeof(params));

params.draw = draw_function;
params.userdata = userdata;

if (!nkui_init(&params)) {
    return 1; /* != 0 means error */
}

while (nkui_events(NKUI_NO_WAIT)) {
    nkui_render();
}

nkui_shutdown();
return 0;
```

The `struct nkui_params` contains more fields to initialize the program. Especially other callback functions aside `draw`.

But in turn, this is a shortcut to more fine grained functions, giving a real control over the application structure. The big interest is that, aside backend specificities (like OpenGL), everything looks the same on any platform, getting different results by using the API differently.

## TODOs

* ~~get rid of the `demo/**/nuklear_xxx.h` backends and rebuild them for usage in nkui, thus avoiding to have static/global variables and be extensible~~
* ~~change the API to use a `nkui` handle overall (needing the previous point to be done) and eventually having several UI instances (each one can have its run loop in a separate thread)~~
* make it multi window (one context per window)
* get rid of standard includes, mostly using `struct nk_allocator` everywhere
* understand all the nuklear font managment (especially baking), I'm always lost in those :)

## Licence

This project is licensed under [MIT](./LICENSE) license, copyright by Richard Gill.
