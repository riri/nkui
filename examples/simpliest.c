#include <stdio.h>
#include <nkui.h>

void draw_ui(struct nk_context *ctx, int width, int height, void *userdata) {
    enum { EASY, HARD };
    static int option = HARD;
    static float value = 0.8f;
    static unsigned int wflags = 0; /* no nuklear window flags by default! */

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

int main(int argc, char *argv[]) { return nkui_run(draw_ui, 0); }
