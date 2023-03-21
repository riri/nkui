#include <nkui.h>
#include "common.h"

struct app { int some_data; };

void draw_ui(struct nkui *ui, struct nk_context *ctx, int width, int height, void *userdata) {
    struct app *app = (struct app*)userdata;
    draw_demo(ctx, nk_rect(0, 0, width, height), 0);
}

int main(int argc, char *argv[]) {
    struct app app = {0};
    return nkui_run(draw_ui, &app);
}
