#ifndef EXAMPLES_COMMON_H
#define EXAMPLES_COMMON_H

#include <nkui.h>
#include <stddef.h>
#include <limits.h>

void draw_demo(struct nk_context *ctx, struct nk_rect bounds, const char *title);
void color_combo(struct nk_context *ctx, float list_width, struct nk_color *value);

enum STYLES {STYLE_BLACK, STYLE_WHITE, STYLE_RED, STYLE_BLUE, STYLE_DARK, NUM_STYLES};

void set_style(struct nk_context *ctx, enum STYLES theme);
nk_bool style_combo(struct nk_context *ctx, float item_height, float list_width, int *theme);

nk_bool overview(struct nk_context *ctx, float width_offset, float height_offset);
nk_bool calculator(struct nk_context *ctx, struct nk_rect bounds);
/* nk_bool calculator(struct nk_context *ctx, float width_offset, float height_offset); */
int node_editor(struct nk_context *ctx, float width_offset, float height_offset);
nk_bool canvas(struct nk_context *ctx, float width_offset, float height_offset);

struct icons {
    struct nk_image desktop;
    struct nk_image home;
    struct nk_image computer;
    struct nk_image directory;

    struct nk_image default_file;
    struct nk_image text_file;
    struct nk_image music_file;
    struct nk_image font_file;
    struct nk_image img_file;
    struct nk_image movie_file;
};

enum file_groups {
    FILE_GROUP_DEFAULT,
    FILE_GROUP_TEXT,
    FILE_GROUP_MUSIC,
    FILE_GROUP_FONT,
    FILE_GROUP_IMAGE,
    FILE_GROUP_MOVIE,
    FILE_GROUP_MAX
};

enum file_types {
    FILE_DEFAULT,
    FILE_TEXT,
    FILE_C_SOURCE,
    FILE_CPP_SOURCE,
    FILE_HEADER,
    FILE_CPP_HEADER,
    FILE_MP3,
    FILE_WAV,
    FILE_OGG,
    FILE_TTF,
    FILE_BMP,
    FILE_PNG,
    FILE_JPEG,
    FILE_PCX,
    FILE_TGA,
    FILE_GIF,
    FILE_MAX
};

struct file_group {
    enum file_groups group;
    const char *name;
    struct nk_image *icon;
};

struct file {
    enum file_types type;
    const char *suffix;
    enum file_groups group;
};

struct media {
    int font;
    int icon_sheet;
    struct icons icons;
    struct file_group group[FILE_GROUP_MAX];
    struct file files[FILE_MAX];
};

struct file_browser {
    /* path */
    char file[PATH_MAX];
    char home[PATH_MAX];
    char desktop[PATH_MAX];
    char directory[PATH_MAX];

    /* directory content */
    char **files;
    char **directories;
    size_t file_count;
    size_t dir_count;
    struct media *media;
};

struct nk_image fb_icon_load(const char *filename);
void fb_media_init(struct nkui *ui, struct media *media);
void fb_media_free(struct nkui *ui, struct media *media);

void file_browser_init(struct file_browser *browser, struct media *media);
void file_browser_free(struct file_browser *browser);
int file_browser_run(struct file_browser *browser, struct nk_context *ctx);

#endif /* EXAMPLES_COMMON_H */
