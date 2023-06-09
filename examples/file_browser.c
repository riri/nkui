#include <nkui.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#ifdef __unix__
#include <dirent.h>
#include <unistd.h>
#endif
#ifndef _WIN32
# include <pwd.h>
#endif

static void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

static char*
str_duplicate(const char *src)
{
    char *ret;
    size_t len = strlen(src);
    if (!len) return 0;
    ret = (char*)malloc(len+1);
    if (!ret) return 0;
    memcpy(ret, src, len);
    ret[len] = '\0';
    return ret;
}

static void
dir_free_list(char **list, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i)
        free(list[i]);
    free(list);
}

static char**
dir_list(const char *dir, int return_subdirs, size_t *count)
{
    size_t n = 0;
    char buffer[PATH_MAX];
    char **results = NULL;
    const DIR *none = NULL;
    size_t capacity = 32;
    size_t size;
    DIR *z;

    assert(dir);
    assert(count);
    strncpy(buffer, dir, PATH_MAX);
    buffer[PATH_MAX - 1] = 0;
    n = strlen(buffer);

    if (n > 0 && (buffer[n-1] != '/'))
        buffer[n++] = '/';

    size = 0;

    z = opendir(dir);
    if (z != none) {
        int nonempty = 1;
        struct dirent *data = readdir(z);
        nonempty = (data != NULL);
        if (!nonempty) return NULL;

        do {
            DIR *y;
            char *p;
            int is_subdir;
            if (data->d_name[0] == '.')
                continue;

            strncpy(buffer + n, data->d_name, PATH_MAX-n);
            y = opendir(buffer);
            is_subdir = (y != NULL);
            if (y != NULL) closedir(y);

            if ((return_subdirs && is_subdir) || (!is_subdir && !return_subdirs)){
                if (!size) {
                    results = (char**)calloc(sizeof(char*), capacity);
                } else if (size >= capacity) {
                    void *old = results;
                    capacity = capacity * 2;
                    results = (char**)realloc(results, capacity * sizeof(char*));
                    assert(results);
                    if (!results) free(old);
                }
                p = str_duplicate(data->d_name);
                results[size++] = p;
            }
        } while ((data = readdir(z)) != NULL);
    }

    if (z) closedir(z);
    *count = size;
    return results;
}

static struct nk_image*
media_icon_for_file(struct media *media, const char *file)
{
    int i = 0;
    const char *s = file;
    char suffix[4];
    int found = 0;
    memset(suffix, 0, sizeof(suffix));

    /* extract suffix .xxx from file */
    while (*s++ != '\0') {
        if (found && i < 3)
            suffix[i++] = *s;

        if (*s == '.') {
            if (found){
                found = 0;
                break;
            }
            found = 1;
        }
    }

    /* check for all file definition of all groups for fitting suffix*/
    for (i = 0; i < FILE_MAX && found; ++i) {
        struct file *d = &media->files[i];
        {
            const char *f = d->suffix;
            s = suffix;
            while (f && *f && *s && *s == *f) {
                s++; f++;
            }

            /* found correct file definition so */
            if (f && *s == '\0' && *f == '\0')
                return media->group[d->group].icon;
        }
    }
    return &media->icons.default_file;
}

static struct file_group
FILE_GROUP(enum file_groups group, const char *name, struct nk_image *icon)
{
    struct file_group fg;
    fg.group = group;
    fg.name = name;
    fg.icon = icon;
    return fg;
}

static struct file
FILE_DEF(enum file_types type, const char *suffix, enum file_groups group)
{
    struct file fd;
    fd.type = type;
    fd.suffix = suffix;
    fd.group = group;
    return fd;
}

void fb_media_init(struct nkui *ui, struct media *media) {
    /* file groups */
    struct icons *icons = &media->icons;
    media->group[FILE_GROUP_DEFAULT] = FILE_GROUP(FILE_GROUP_DEFAULT,"default",&icons->default_file);
    media->group[FILE_GROUP_TEXT] = FILE_GROUP(FILE_GROUP_TEXT, "textual", &icons->text_file);
    media->group[FILE_GROUP_MUSIC] = FILE_GROUP(FILE_GROUP_MUSIC, "music", &icons->music_file);
    media->group[FILE_GROUP_FONT] = FILE_GROUP(FILE_GROUP_FONT, "font", &icons->font_file);
    media->group[FILE_GROUP_IMAGE] = FILE_GROUP(FILE_GROUP_IMAGE, "image", &icons->img_file);
    media->group[FILE_GROUP_MOVIE] = FILE_GROUP(FILE_GROUP_MOVIE, "movie", &icons->movie_file);

    /* files */
    media->files[FILE_DEFAULT] = FILE_DEF(FILE_DEFAULT, NULL, FILE_GROUP_DEFAULT);
    media->files[FILE_TEXT] = FILE_DEF(FILE_TEXT, "txt", FILE_GROUP_TEXT);
    media->files[FILE_C_SOURCE] = FILE_DEF(FILE_C_SOURCE, "c", FILE_GROUP_TEXT);
    media->files[FILE_CPP_SOURCE] = FILE_DEF(FILE_CPP_SOURCE, "cpp", FILE_GROUP_TEXT);
    media->files[FILE_HEADER] = FILE_DEF(FILE_HEADER, "h", FILE_GROUP_TEXT);
    media->files[FILE_CPP_HEADER] = FILE_DEF(FILE_HEADER, "hpp", FILE_GROUP_TEXT);
    media->files[FILE_MP3] = FILE_DEF(FILE_MP3, "mp3", FILE_GROUP_MUSIC);
    media->files[FILE_WAV] = FILE_DEF(FILE_WAV, "wav", FILE_GROUP_MUSIC);
    media->files[FILE_OGG] = FILE_DEF(FILE_OGG, "ogg", FILE_GROUP_MUSIC);
    media->files[FILE_TTF] = FILE_DEF(FILE_TTF, "ttf", FILE_GROUP_FONT);
    media->files[FILE_BMP] = FILE_DEF(FILE_BMP, "bmp", FILE_GROUP_IMAGE);
    media->files[FILE_PNG] = FILE_DEF(FILE_PNG, "png", FILE_GROUP_IMAGE);
    media->files[FILE_JPEG] = FILE_DEF(FILE_JPEG, "jpg", FILE_GROUP_IMAGE);
    media->files[FILE_PCX] = FILE_DEF(FILE_PCX, "pcx", FILE_GROUP_IMAGE);
    media->files[FILE_TGA] = FILE_DEF(FILE_TGA, "tga", FILE_GROUP_IMAGE);
    media->files[FILE_GIF] = FILE_DEF(FILE_GIF, "gif", FILE_GROUP_IMAGE);
}

void fb_media_free(struct nkui *ui, struct media *media) {
    int i, max;

    for (i = 0; i < FILE_GROUP_MAX; ++i) {
        if (media->group[i].icon->handle.ptr) nkui_image_free(ui, *(media->group[i].icon));
    }
    for (i = 0; i < FILE_MAX; ++i) {
    }
}

static void file_browser_reload_directory_content(struct file_browser *browser, const char *path)
{
    strncpy(browser->directory, path, PATH_MAX);
    browser->directory[PATH_MAX - 1] = 0;
    dir_free_list(browser->files, browser->file_count);
    dir_free_list(browser->directories, browser->dir_count);
    browser->files = dir_list(path, 0, &browser->file_count);
    browser->directories = dir_list(path, 1, &browser->dir_count);
}

void file_browser_init(struct file_browser *browser, struct media *media)
{
    memset(browser, 0, sizeof(*browser));
    browser->media = media;
    {
        /* load files and sub-directory list */
        const char *dir = getenv("HOME");
#ifdef _WIN32
        if (!dir) dir = getenv("USERPROFILE");
#else
        if (!dir) dir = getpwuid(getuid())->pw_dir;
        {
            size_t l;
            strncpy(browser->home, dir, PATH_MAX);
            browser->home[PATH_MAX - 1] = 0;
            l = strlen(browser->home);
            strcpy(browser->home + l, "/");
            strcpy(browser->directory, browser->home);
        }
#endif
        {
            size_t l;
            strcpy(browser->desktop, browser->home);
            l = strlen(browser->desktop);
            strcpy(browser->desktop + l, "desktop/");
        }
        browser->files = dir_list(browser->directory, 0, &browser->file_count);
        browser->directories = dir_list(browser->directory, 1, &browser->dir_count);
    }
}

void
file_browser_free(struct file_browser *browser)
{
    if (browser->files)
        dir_free_list(browser->files, browser->file_count);
    if (browser->directories)
        dir_free_list(browser->directories, browser->dir_count);
    browser->files = NULL;
    browser->directories = NULL;
    memset(browser, 0, sizeof(*browser));
}

static
int cmp_fn(const void *str1, const void *str2)
{
    const char *str1_ret = *(const char **)str1;
    const char *str2_ret = *(const char **)str2;
    return nk_stricmp(str1_ret, str2_ret);
}

int file_browser_run(struct file_browser *browser, struct nk_context *ctx) {
    int ret = 0;
    struct media *media = browser->media;
    struct nk_rect total_space;
    static nk_bool file_browser_is_open = nk_true;

    if (file_browser_is_open)
    {
        if (nk_begin(ctx, "File Browser", nk_rect(50, 50, 600, 400),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|NK_WINDOW_NO_SCROLLBAR|
                NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            static float ratio[] = {0.25f, NK_UNDEFINED};
            float spacing_x = ctx->style.window.spacing.x;

            /* output path directory selector in the menubar */
            ctx->style.window.spacing.x = 0;
            nk_menubar_begin(ctx);
            {
                char *d = browser->directory;
                char *begin = d + 1;
                nk_layout_row_dynamic(ctx, 25, 6);
                while (*d++) {
                    if (*d == '/') {
                        *d = '\0';
                        if (nk_button_label(ctx, begin)) {
                            *d++ = '/'; *d = '\0';
                            file_browser_reload_directory_content(browser, browser->directory);
                            break;
                        }
                        *d = '/';
                        begin = d + 1;
                    }
                }
            }
            nk_menubar_end(ctx);
            ctx->style.window.spacing.x = spacing_x;

            /* window layout */
            total_space = nk_window_get_content_region(ctx);
            nk_layout_row(ctx, NK_DYNAMIC, total_space.h - 40, 2, ratio);

            nk_group_begin(ctx, "Special", NK_WINDOW_NO_SCROLLBAR);
            {
                struct nk_image home = media->icons.home;
                struct nk_image desktop = media->icons.desktop;
                struct nk_image computer = media->icons.computer;

                nk_layout_row_dynamic(ctx, 40, 1);
                if (nk_button_image_label(ctx, home, "home", NK_TEXT_CENTERED))
                    file_browser_reload_directory_content(browser, browser->home);
                /*if (nk_button_image_label(ctx,desktop,"desktop",NK_TEXT_CENTERED))
                    file_browser_reload_directory_content(browser, browser->desktop);*/
                if (nk_button_image_label(ctx,computer,"computer",NK_TEXT_CENTERED))
                    file_browser_reload_directory_content(browser, "/");
                nk_group_end(ctx);
            }

            /* output directory content window */
            nk_group_begin(ctx, "Content", NK_WINDOW_BORDER);
            {
                int index = -1;
                size_t i = 0, j = 0;
                size_t rows = 0, cols = 0;
                size_t count = browser->dir_count + browser->file_count;

                /* File icons layout */
                cols = 2;
                rows = count / cols;
                static float ratio2[] = {0.08f, NK_UNDEFINED};
                nk_layout_row(ctx, NK_DYNAMIC, 30, 2, ratio2);
                for (i = 0; i <= rows; i += 1) {
                    size_t n = j + cols;
                    for (; j < count && j < n; ++j) {
                        /* draw one column of icons */
                        if (j < browser->dir_count) {
                            /* draw and execute directory buttons */
                            if (nk_button_image(ctx,media->icons.directory))
                                index = (int)j;

                            qsort(browser->directories, browser->dir_count, sizeof(char *), cmp_fn);
                            nk_label(ctx, browser->directories[j], NK_TEXT_LEFT);
                        } else {
                            /* draw and execute files buttons */
                            struct nk_image *icon;
                            size_t fileIndex = ((size_t)j - browser->dir_count);
                            icon = media_icon_for_file(media,browser->files[fileIndex]);
                            if (nk_button_image(ctx, *icon)) {
                                strncpy(browser->file, browser->directory, PATH_MAX);
                                n = strlen(browser->file);
                                strncpy(browser->file + n, browser->files[fileIndex], PATH_MAX - n);
                                ret = 1;
                            }
                        }
                        /* draw one column of labels */
                        if (j >= browser->dir_count) {
                            size_t t = j - browser->dir_count;
                            qsort(browser->files, browser->file_count, sizeof(char *), cmp_fn);
                            nk_label(ctx,browser->files[t],NK_TEXT_LEFT);
                        }
                    }
                }

                if (index != -1) {
                    size_t n = strlen(browser->directory);
                    strncpy(browser->directory + n, browser->directories[index], PATH_MAX - n);
                    n = strlen(browser->directory);
                    if (n < PATH_MAX - 1) {
                        browser->directory[n] = '/';
                        browser->directory[n+1] = '\0';
                    }
                    file_browser_reload_directory_content(browser, browser->directory);
                }
                nk_group_end(ctx);
            }

            nk_layout_row_dynamic(ctx, 30, 5);
            nk_label(ctx,"",NK_TEXT_LEFT);
            nk_label(ctx,"",NK_TEXT_LEFT);
            nk_label(ctx,"",NK_TEXT_LEFT);
            if(nk_button_label(ctx, "Cancel"))
            {
                fprintf(stdout, "File dialog has been closed!\n");
                file_browser_is_open = nk_false;
            }
            if(nk_button_label(ctx, "Open"))
                fprintf(stdout, "Insert routine to open/save the file!\n");
        }
        nk_end(ctx);
    }
    return file_browser_is_open;
}
