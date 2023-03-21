#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nkui.h>
#include "common.h"

enum {
    DEMO_OVERVIEW,
    DEMO_CALCULATOR,
    DEMO_NODEEDITOR,
    DEMO_CANVAS,
    DEMO_FILEBROWSER,
    NUM_DEMOS
};

struct app_font {
    const char *name;
    int size;
    struct nk_user_font *font;
};

struct app {
    struct nkui *ui;
    struct nk_color bg;
    struct app_font font;

    struct media media;
    struct file_browser browser;
};

#define APP_GET(v, a) struct app *v = (struct app *)(a)

static nk_bool app_init(struct nkui *ui, struct nk_context *ctx, void *userdata) {
    APP_GET(app, userdata);
    NK_UNUSED(ctx);
    memset(app, 0, sizeof(struct app));
    app->ui = ui;
    app->bg = nk_rgba(32, 32, 32, 255);

    app->media.icons.home = nkui_image_load_file(app->ui, "../icon/home.png");
    app->media.icons.directory = nkui_image_load_file(app->ui, "../icon/directory.png");
    app->media.icons.computer = nkui_image_load_file(app->ui, "../icon/computer.png");
    app->media.icons.desktop = nkui_image_load_file(app->ui, "../icon/desktop.png");
    app->media.icons.default_file = nkui_image_load_file(app->ui, "../icon/default.png");
    app->media.icons.text_file = nkui_image_load_file(app->ui, "../icon/text.png");
    app->media.icons.music_file = nkui_image_load_file(app->ui, "../icon/music.png");
    app->media.icons.font_file =  nkui_image_load_file(app->ui, "../icon/font.png");
    app->media.icons.img_file = nkui_image_load_file(app->ui, "../icon/img.png");
    app->media.icons.movie_file = nkui_image_load_file(app->ui, "../icon/movie.png");
    fb_media_init(app->ui, &(app->media));
    file_browser_init(&(app->browser), &(app->media));

    app->font.name = "Sans";
    app->font.size = 10;
    app->font.font = nkui_font_load_native(app->ui, app->font.name, app->font.size);
    nk_style_set_font(ctx, app->font.font);

    return nk_true;
}

static void app_term(struct nkui *ui, void *userdata) {
    APP_GET(app, userdata);
    file_browser_free(&app->browser);
    fb_media_free(app->ui, &(app->media));
}

static struct nk_color app_clear(void *userdata) {
    APP_GET(app, userdata);
    return app->bg;
}

#define SIDEBAR_WIDTH   250
#define ROW_HEIGHT      24

nk_bool demo_opened(struct nk_context *ctx, const char *name) {
    struct nk_window *win = nk_window_find(ctx, name);
    return (win && !(win->flags & NK_WINDOW_HIDDEN));
}

static void app_draw(struct nkui *ui, struct nk_context *ctx, int w, int h, void *userdata) {
    APP_GET(app, userdata);
    static const struct nk_vec2 sidebar_padding = {15, 15};
    static nk_bool style_sidebar = nk_false;
    static int style = STYLE_BLACK;
    static const char *DEMO_LABELS[] = {"Overview","Calculator","Node editor","Canvas","File browser"};
    static nk_bool file_browser_is_opened = nk_false;
    static nk_bool overview_initial = nk_true;
    nk_bool demos[] = {
        overview_initial || demo_opened(ctx, DEMO_LABELS[DEMO_OVERVIEW]),
        demo_opened(ctx, DEMO_LABELS[DEMO_CALCULATOR]),
        demo_opened(ctx, DEMO_LABELS[DEMO_NODEEDITOR]),
        demo_opened(ctx, DEMO_LABELS[DEMO_CANVAS]),
        file_browser_is_opened
    };
    int i;

    overview_initial = nk_false;

    if (!style_sidebar) nk_style_default(ctx);

    /* demo control on the left sidebar */
    nk_style_push_vec2(ctx, &(ctx->style.window.padding), sidebar_padding);
    if (nk_begin(ctx, "", nk_rect(0, 0, SIDEBAR_WIDTH, h), NK_WINDOW_BACKGROUND)) {
        nk_layout_row_static(ctx, ROW_HEIGHT, 80, 1);
        if (nk_button_label(ctx, "Quit")) {
            nkui_stop(app->ui);
        }

        nk_spacer(ctx);
        nk_layout_row_dynamic(ctx, ROW_HEIGHT, 1);
        nk_label(ctx, "Background color:", NK_TEXT_ALIGN_LEFT);
        float widget_width = nk_widget_width(ctx);
        color_combo(ctx, widget_width, &(app->bg));
        nk_spacer(ctx);
        nk_label(ctx, "Style:", NK_TEXT_ALIGN_LEFT);
        if (style_combo(ctx, ROW_HEIGHT, widget_width, &style)) {
            set_style(ctx, style);
        }
        style_sidebar = nk_check_label(ctx, "On sidebar too", style_sidebar);

        nk_spacer(ctx);
        nk_label(ctx, "Font:", NK_TEXT_LEFT);
        int font_size = app->font.size;
        nk_property_int(ctx, "Size:", 8, &font_size, 96, 1, 1);
        if (font_size != app->font.size) {
            struct nk_user_font *font = nkui_font_load_native(app->ui, app->font.name, font_size);
            nkui_font_free(app->ui, app->font.font);
            nk_style_set_font(ctx, font);
            app->font.font = font;
            app->font.size = font_size;
        }

        nk_spacer(ctx);
        nk_label(ctx, "Demos:", NK_TEXT_ALIGN_LEFT);
        for (i = 0; i < NUM_DEMOS; ++i) {
            demos[i] = nk_check_label(ctx, DEMO_LABELS[i], demos[i]);
        }
    }
    nk_end(ctx);
    nk_style_pop_vec2(ctx);
    set_style(ctx, style);

    if (demos[DEMO_OVERVIEW]) overview(ctx, SIDEBAR_WIDTH + 20, 20);
    if (demos[DEMO_CALCULATOR]) calculator(ctx, nk_rect(0, h - 200, SIDEBAR_WIDTH, 200));
    if (demos[DEMO_NODEEDITOR]) node_editor(ctx, SIDEBAR_WIDTH + 20, 20);
    if (demos[DEMO_CANVAS]) canvas(ctx, SIDEBAR_WIDTH + 20, 20);
    if (demos[DEMO_FILEBROWSER]) {
        file_browser_is_opened = file_browser_run(&(app->browser), ctx);
    }
}

int main(int argc, char *argv[]) {
    struct app app;
    struct nkui_params params = {
        "All Nuklear demos",
        0, 0, 1024, 768,
        app_draw, app_clear, app_init, app_term,
        &app
    };

    app.ui = nkui_init(&params);
    if (!app.ui) {
        return EXIT_FAILURE;
    }

    while(nkui_events(app.ui, NKUI_NO_WAIT)) {
        nkui_render(app.ui);
    }

    nkui_shutdown(app.ui);
    return EXIT_SUCCESS;
}