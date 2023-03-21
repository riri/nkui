#ifndef NKUI_H
#define NKUI_H

#define NKUI_XLIB   'x'
#define NKUI_GDIP   'w'
#define NKUI_GLFW   'g'
#define NKUI_SDL2   's'
#if NKUI_BACKEND==NKUI_NATIVE
#undef NKUI_BACKEND
#ifdef _WIN32
#define NKUI_BACKEND NKUI_GDIP
#elif defined(__apple__)
#else
#define NKUI_BACKEND NKUI_XLIB
#endif
#endif

/* Include nuklear before this header if you want to control its inclusion
 * and defines. Nkui will automatically set the following defines depending
 * on its needs for a particular backend and opengl usage:
 * 
 */
#ifndef NK_NUKLEAR_H_

#if defined(NKUI_IMPLEMENTATION) && !defined(NKUI_NO_NK_IMPLEMENTATION) && !defined(NK_IMPLEMENTATION)
#define NK_IMPLEMENTATION
#endif

#if NKUI_BACKEND==NKUI_XLIB
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_VARARGS
/* define as dummies as they are not used and generate warnings */
#define NK_INV_SQRT
#define NK_SIN
#define NK_COS
#endif

#include <nuklear.h>

#endif /* NK_NUKLEAR_H_ */

#ifndef NKUI_API
#define NKUI_API NK_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* API ***********************************************************************/

/* implementation defined: handle to one nkui instance */
struct nkui;

/* nkui_draw_fn is a callback function you provide to draw your nuklear UI */
typedef void (*nkui_draw_fn)(struct nkui *ui, struct nk_context *ctx, int width, int height, void *userdata);
/* nkui_clear_fn is called before rendering the UI, returning the background color */
typedef struct nk_color (*nkui_clear_fn)(void *userdata);
/* nkui_init_fn is called just before the first event is checked, the window is create */
typedef nk_bool (*nkui_init_fn)(struct nkui *ui, struct nk_context *ctx, void *userdata);
/* nkui_term_fn is called just before the window is destroyed */
typedef void (*nkui_term_fn)(struct nkui *ui, void *userdata);

/* nkui_run() is the simplified version of nkui to make quick and dirty projects.
 * main() can become a one liner: int main(){return nkui_run(your_draw_function, NULL);}
 */
NKUI_API int nkui_run(nkui_draw_fn draw_function, void *userdata);

/* those two constants are used with nkui_events() to select the event handling type */
enum {
    NKUI_WAIT_FOREVER   = -1,   /* block on events */
    NKUI_NO_WAIT        = 0     /* run continuesly */
};

/* nkui_params structure defines window creation parameters when calling
 * nkui_init()
 */
struct nkui_params {
    const char *title;
    int x;
    int y;
    int width;
    int height;

    nkui_draw_fn draw;
    nkui_clear_fn clear;
    nkui_init_fn init;
    nkui_term_fn term;

    void *userdata;

    /* for phase2, just a reminder for when we do our own backends
    struct nk_allocator *allocator; */
};

/* nkui_init() setup the backend and creates the window */
NKUI_API struct nkui *nkui_init(struct nkui_params *params);
/* nkui_shutdown() cleans up all nkui resources (and destroys the window) */
NKUI_API void nkui_shutdown(struct nkui *ui);
/* nkui_events() handles events and populates nuklear input context
 * it returns nk_false if the program should stop
 * use NKUI_NO_WAIT for a classic continuous loop, NKUI_WAIT_FOREVER to block
 * on events (more desktop like) or any value > 0 to wait for this amount of
 * milliseconds
 */
NKUI_API nk_bool nkui_events(struct nkui *ui, int wait);
/* nkui_stop() permits to end the  program from a function */
NKUI_API void nkui_stop(struct nkui *ui);
/* nkui_render() clears the window, calls your draw function, then renders
 * the result on the window */
NKUI_API void nkui_render(struct nkui *ui);

NKUI_API struct nk_font_atlas *nkui_font_begin(struct nkui *ui);
#ifdef NK_INCLUDE_DEFAULT_FONT
/* NKUI_API struct nk_font *nkui_font_load_default(int size); */
#endif
#if NKUI_BACKEND==NKUI_XLIB
/* NKUI_API struct nkui_font *nkui_font_load_native(const char *name, int size); */
NKUI_API struct nk_user_font *nkui_font_load_native(struct nkui *ui, const char *name, int size);
#endif
#ifdef NK_INCLUDE_STANDARD_IO
NKUI_API struct nk_font *nkui_font_load_file(struct nkui *ui, const char *name, int size);
#endif
NKUI_API void nkui_font_end(struct nkui *ui);
NKUI_API void nkui_font_free(struct nkui *ui, struct nk_user_font *font);

#ifdef NKUI_IMAGE_LOADER
NKUI_API struct nk_image nkui_image_load_file(struct nkui *ui, const char *filename);
NKUI_API struct nk_image nkui_image_load_memory(struct nkui *ui, const unsigned char *membuf, nk_uint memsize);
NKUI_API void nkui_image_free(struct nkui *ui, struct nk_image image);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NKUI_H */

/*****************************************************************************/
/* IMPLEMENTATION                                                            */
/*****************************************************************************/
#if defined(NKUI_IMPLEMENTATION) && !defined(NKUI_IMPLEMENTED)
#define NKUI_IMPLEMENTED

#ifndef NKUI_DEFAULT_TITLE
#define NKUI_DEFAULT_TITLE      "Nuklear Window"
#endif
#ifndef NKUI_DEFAULT_WIDTH
#define NKUI_DEFAULT_WIDTH      1024
#endif
#ifndef NKUI_DEFAULT_HEIGHT
#define NKUI_DEFAULT_HEIGHT     768
#endif
#ifndef NKUI_DEFAULT_BG
#define NKUI_DEFAULT_BG         {0, 20, 20, 255}
#endif
#ifndef NKUI_DEFAULT_FONT_SIZE
#define NKUI_DEFAULT_FONT_SIZE  12
#endif
#ifndef NKUI_DEFAULT_FONT_NATIVE
#define NKUI_DEFAULT_FONT_NATIVE    "Arial"
#endif

#ifndef NKUI_CURVE_SEGMENTS
#define NKUI_CURVE_SEGMENTS     22
#endif

#ifndef NK_IMPLEMENTATION
#include <string.h>
#ifndef NK_MEMCPY
#define NK_MEMCPY               memcpy
#endif
#ifndef NK_MEMSET
#define NK_MEMSET               memset
#endif
#endif

#if defined(NKUI_REALLOC) && !defined(NKUI_FREE) || !defined(NKUI_REALLOC) && defined(NKUI_FREE)
#error NKUI_REALLOC and NKUI_FREE must be defined together, or not at all.
#endif
#ifndef NKUI_REALLC
#include <stdlib.h>
#define NKUI_REALLOC(p, s)      realloc(p, s)
/* note that NKUI_FREE() returns a nullified pointer */
#define NKUI_FREE(p)            (free(p), (p) = 0)
#endif

#if !defined(NKUI_MEMCPY) || !defined(NKUI_MEMSET)
#if !defined(NK_MEMCPY) && !defined(NK_MEMSET)
#include <string.h>
#endif
#ifndef NKUI_MEMCPY
#ifdef NK_MEMCPY
#define NKUI_MEMCPY             NK_MEMCPY
#else
#define NKUI_MEMCPY             memcpy
#endif
#endif
#ifndef NKUI_MEMSET
#ifdef NK_MEMSET
#define NKUI_MEMSET             NK_MEMSET
#else
#define NKUI_MEMSET             memset
#endif
#endif

#ifndef NKUI_ZERO
#define NKUI_ZERO(p, s)         NKUI_MEMSET(p, 0, s)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

static struct nk_color nkui_default_render_color(void *userdata) {
    static struct nk_color color = NKUI_DEFAULT_BG;
    NK_UNUSED(userdata);
    return color;
}

static struct nkui_params nkui_default_params = {
    NKUI_DEFAULT_TITLE,
    0, 0, NKUI_DEFAULT_WIDTH, NKUI_DEFAULT_HEIGHT,

    NULL, nkui_default_render_color, NULL, NULL,
    NULL
};


/* API ***********************************************************************/

NKUI_API int nkui_run(nkui_draw_fn draw_function, void *userdata) {
    struct nkui_params params;
    struct nkui *ui;
    NKUI_MEMCPY(&params, &nkui_default_params, sizeof(params));
    params.draw = draw_function;
    params.userdata = userdata;
    ui = nkui_init(&params);
    if (!ui) return 1;
    while (nkui_events(ui, NKUI_NO_WAIT)) nkui_render(ui);
    nkui_shutdown(ui);
    return 0;
}

#ifdef __cplusplus
}
#endif

/* if no backend is defined, user has to include the correct backend header
 * in their project instead of this one */
#ifdef NKUI_BACKEND
#if NKUI_BACKEND==NKUI_XLIB
#include "nkui_xlib.h"
#endif
#endif /* NKUI_BACKEND */

#endif /* NKUI_IMPLEMENTATION */
