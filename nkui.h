#ifndef NKUI_H
#define NKUI_H

#define NKUI_XLIB   'x'
#define NKUI_GDIP   'w'
#define NKUI_GLFW   'g'
#define NKUI_SDL2   's'

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

struct nkui_font;

/* nkui_draw_fn is a callback function you provide to draw your nuklear UI */
typedef void (*nkui_draw_fn)(struct nk_context *ctx, int width, int height, void *userdata);
/* nkui_clear_fn is called before rendering the UI, returning the background color */
typedef struct nk_color (*nkui_clear_fn)(void *userdata);
/* nkui_init_fn is called just before the first event is checked, the window is create */
typedef nk_bool (*nkui_init_fn)(struct nk_context *ctx, void *userdata);
/* nkui_term_fn is called just before the window is destroyed */
typedef void (*nkui_term_fn)(void *userdata);

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
NKUI_API nk_bool nkui_init(struct nkui_params *params);
/* nkui_shutdown() cleans up all nkui resources (and destroys the window) */
NKUI_API void nkui_shutdown(void);
/* nkui_events() handles events and populates nuklear input context
 * it returns nk_false if the program should stop
 * use NKUI_NO_WAIT for a classic continuous loop, NKUI_WAIT_FOREVER to block
 * on events (more desktop like) or any value > 0 to wait for this amount of
 * milliseconds
 */
NKUI_API nk_bool nkui_events(int wait);
/* nkui_render() clears the window, calls your draw function, then renders
 * the result on the window */
NKUI_API void nkui_render(void);

NKUI_API struct nk_font_atlas *nkui_font_begin(void);
#ifdef NK_INCLUDE_DEFAULT_FONT
NKUI_API struct nk_font *nkui_font_load_default(int size);
#endif
#if NKUI_BACKEND==NKUI_XLIB
NKUI_API struct nkui_font *nkui_font_load_native(const char *name, int size);
#endif
#ifdef NK_INCLUDE_STANDARD_IO
NKUI_API struct nk_font *nkui_font_load_file(const char *name, int size);
#endif
NKUI_API void nkui_font_end(void);

#ifdef __cplusplus
}
#endif

#endif /* NKUI_H */

/*****************************************************************************/
/* IMPLEMENTATION                                                            */
/*****************************************************************************/
#if defined(NKUI_IMPLEMENTATION) && !defined(NKUI_IMPLEMENTED)
#define NKUI_IMPLEMENTED

#ifdef __cplusplus
extern "C" {
#endif

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
#define NKUI_DEFAULT_BG         (struct nk_color){0, 20, 20, 255}
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
#ifndef nk_zero
#define nk_zero(p, s)           NK_MEMSET(p, 0, s)
#endif
#endif

static struct nk_color nkui_default_render_color(void *userdata) {
    NK_UNUSED(userdata);
    return NKUI_DEFAULT_BG;
}

static struct nkui_params nkui_default_params = {
    .title  = NKUI_DEFAULT_TITLE,
    .x      = 0,
    .y      = 0,
    .width  = NKUI_DEFAULT_WIDTH,
    .height = NKUI_DEFAULT_HEIGHT,

    .clear  = nkui_default_render_color
};


/* API ***********************************************************************/

NKUI_API int nkui_run(nkui_draw_fn draw_function, void *userdata) {
    struct nkui_params params;
    NK_MEMCPY(&params, &nkui_default_params, sizeof(params));
    params.draw = draw_function;
    params.userdata = userdata;
    if (!nkui_init(&params)) return 1;
    while (nkui_events(NKUI_NO_WAIT)) nkui_render();
    nkui_shutdown();
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
