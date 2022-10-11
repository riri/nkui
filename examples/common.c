#include <nkui.h>

enum { EASY, HARD };

void draw_demo(struct nk_context *ctx, struct nk_rect bounds, const char *title) {
    static int option = HARD;
    static float value = 0.8f;
    static unsigned int wflags = 0; /* no nuklear window flags by default! */
    if (title) wflags = NK_WINDOW_BORDER|NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE
                      | NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|NK_WINDOW_TITLE;
    else title = "";

    if (nk_begin(ctx, title, bounds, 0)) {
        nk_layout_row_static(ctx, 30, 80, 1);
        if (nk_button_label(ctx, "button")) {
            /* show a popup */
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