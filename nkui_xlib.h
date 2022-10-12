#ifndef NKUI_XLIB_H
#define NKUI_XLIB_H

#include "nkui.h"

#ifdef NKUI_IMPLEMENTATION

#define NK_XLIB_IMPLEMENTATION
#define NK_XLIB_USE_XFT

#if defined(NKUI_IMAGE_LOADER) && !defined(NKUI_NO_INCLUDE_STBIMAGE)
#define NK_XLIB_INCLUDE_STB_IMAGE
#ifndef NKUI_NO_STBIMAGE_IMPL
#define NK_XLIB_IMPLEMENT_STB_IMAGE
#endif
/*#include <stb_image.h>*/
#endif /* NKUI_IMAGE_LOADED && !NKUI_NO_INCLUDE_STBIMAGE */

#include <time.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xft/Xft.h>
#include <demo/x11_xft/nuklear_xlib.h>

#ifndef NKUI_MINLOOP_DELAY
#define NKUI_MINLOOP_DELAY      20
#endif
#if 0
#ifndef NKUI_DOUBLECLICK_LO
#define NKUI_DOUBLECLICK_LO     20
#endif
#ifndef NKUI_DOUBLECLICK_HI
#define NKUI_DOUBLECLICK_HI     200
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct nkui_font {
    XFont *xfont;
};

static struct nkui {
    Display *dpy;
    Window root;
    Visual *vis;
    Colormap cmap;
    int screen;
    int depth;

    Window xwin;
    Cursor hidden_cursor;
    XWindowAttributes xwa;
    long loop_started;
    nk_bool stop;

    struct atoms {
        Atom wm_protocols;
        Atom wm_delete_window;
    } atoms;

    struct nk_context *ctx;
    /*struct nk_font_atlas *atlas;*/
    struct nkui_font font;

    nkui_draw_fn draw;
    nkui_clear_fn clear;
    nkui_init_fn init;
    nkui_term_fn term;
    void *userdata;
} nkui;

static long timestamp(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return 0;
    return (long)((long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
}

static void sleep_for(long t) {
    struct timespec ts;
    const time_t sec = (int)(t / 1000);
    const long ms = t - (sec * 1000);
    ts.tv_sec = sec;
    ts.tv_nsec = ms * 1000000L;
    while (-1 == nanosleep(&ts, &ts));
}


/* API ***********************************************************************/

NKUI_API nk_bool nkui_init(struct nkui_params *params) {
    XSetWindowAttributes xswa;
    int width; int height;
    const char *title;

    NK_ASSERT(params);

    nk_zero(&nkui, sizeof(nkui));
    nkui.dpy = XOpenDisplay(NULL);
    if (!nkui.dpy) return nk_false;
    nkui.screen = XDefaultScreen(nkui.dpy);
    nkui.depth = DefaultDepth(nkui.dpy, nkui.screen);
    nkui.root = RootWindow(nkui.dpy, nkui.screen);
    nkui.vis = DefaultVisual(nkui.dpy, nkui.screen);
    nkui.cmap = DefaultColormap(nkui.dpy, nkui.screen);

    nkui.atoms.wm_protocols = XInternAtom(nkui.dpy, "WM_PROTOCOLS", False);
    nkui.atoms.wm_delete_window = XInternAtom(nkui.dpy, "WM_DELETE_WINDOW", False);

    nkui.draw = params->draw;
    nkui.clear = params->clear ? params->clear : nkui_default_render_color;
    nkui.init = params->init;
    nkui.term = params->term;
    nkui.userdata = params->userdata;

    width = params->width > 0 ? params->width : NKUI_DEFAULT_WIDTH;
    height = params->height > 0 ? params->height : NKUI_DEFAULT_HEIGHT;
    title = params->title ? params->title : NKUI_DEFAULT_TITLE;
    xswa.event_mask =   ExposureMask | StructureNotifyMask
                    |   KeyPressMask | KeyReleaseMask | KeymapStateMask
                    |   ButtonPressMask | ButtonReleaseMask | ButtonMotionMask
                    |   PointerMotionMask
                    ;
    nkui.xwin = XCreateWindow(nkui.dpy, nkui.root, params->x, params->y, width, height,
        0, nkui.depth, InputOutput, nkui.vis, CWEventMask, &xswa);
    XStoreName(nkui.dpy, nkui.xwin, title);
    XMapWindow(nkui.dpy, nkui.xwin);
    {
        Atom protocols[] = { nkui.atoms.wm_delete_window };
        XSetWMProtocols(nkui.dpy, nkui.xwin, protocols, sizeof(protocols)/sizeof(Atom));
    }
    XGetWindowAttributes(nkui.dpy, nkui.xwin, &nkui.xwa);

    {
        static XColor dummy;
        static char d[1] = {0};
        Pixmap blank = XCreateBitmapFromData(nkui.dpy, nkui.xwin, d, 1, 1);
        nkui.hidden_cursor = XCreatePixmapCursor(nkui.dpy, blank, blank, &dummy, &dummy, 0, 0);
        XFreePixmap(nkui.dpy, blank);
    }

    return nk_true;
}

NKUI_API void nkui_shutdown(void) {
    nk_xfont_del(nkui.dpy, nkui.font.xfont);
    nk_xlib_shutdown();
    XFreeCursor(nkui.dpy, nkui.hidden_cursor);
    XUnmapWindow(nkui.dpy, nkui.xwin);
    XDestroyWindow(nkui.dpy, nkui.xwin);
    XCloseDisplay(nkui.dpy);
}

static void nkui_ensure_context_loaded(void) {
    if (!nkui.font.xfont) {
        nkui_font_load_native(NKUI_DEFAULT_FONT_NATIVE, NKUI_DEFAULT_FONT_SIZE);
    }
    if (!nkui.ctx) {
        nkui.ctx = nk_xlib_init(nkui.font.xfont, nkui.dpy, nkui.screen, nkui.xwin,
            nkui.vis, nkui.cmap, nkui.xwa.width, nkui.xwa.height);
        if (nkui.init) {
            if (!nkui.init(nkui.ctx, nkui.userdata)) {
                /* something went wrong, cleanup everything */
                nkui_shutdown();
            }
        }
    }
}

NKUI_API nk_bool nkui_events(int wait) {
    nk_bool ret = nk_true;
    XEvent event;

    NK_UNUSED(wait);
    if (nkui.stop) return nk_false;

    /* if no context, then no font loaded, ensure all is loaded now */
    if (!nkui.ctx) nkui_ensure_context_loaded();

    nkui.loop_started = timestamp();
    nk_input_begin(nkui.ctx);
    while (!nkui.stop && ret && XPending(nkui.dpy)) {
        XNextEvent(nkui.dpy, &event);
        if (event.type == ClientMessage) {
            if (event.xclient.message_type == nkui.atoms.wm_protocols &&
                event.xclient.data.l[0] == (long)nkui.atoms.wm_delete_window) {
                ret = nk_false;
                break;
            }
        }
        if (XFilterEvent(&event, nkui.xwin)) continue;
        nk_xlib_handle_event(nkui.dpy, nkui.screen, nkui.xwin, &event);
    }
    nk_input_end(nkui.ctx);
    return ret;
}

NKUI_API void nkui_stop(void) {
    nkui.stop = nk_true;
}

NKUI_API void nkui_render(void) {
    long dt;

    XGetWindowAttributes(nkui.dpy, nkui.xwin, &nkui.xwa);
    nkui.draw(nkui.ctx, nkui.xwa.width, nkui.xwa.height, nkui.userdata);
    if (nkui.stop) return;

    XClearWindow(nkui.dpy, nkui.xwin);
    nk_xlib_render(nkui.xwin, nkui.clear(nkui.userdata));
    XFlush(nkui.dpy);

    dt = timestamp() - nkui.loop_started;
    if (dt < NKUI_MINLOOP_DELAY) sleep_for(NKUI_MINLOOP_DELAY - dt);
}

NKUI_API struct nk_font_atlas *nkui_font_begin(void) {
    return NULL;
}

#ifdef NK_INCLUDE_DEFAULT_FONT
NKUI_API struct nk_font *nkui_font_load_default(int size) {
    NK_UNUSED(size);
    return NULL;
}
#endif

#ifdef NK_INCLUDE_STANDARD_IO
NKUI_API struct nk_font *nkui_font_load_file(const char *name, int size) {
    NK_UNUSED(name);
    NK_UNUSED(size);
    return NULL;
}
#endif

NKUI_API struct nkui_font *nkui_font_load_native(const char *name, int size) {
    char xftname[128];
    snprintf(xftname, 128, "%s-%d", name, size);
    nkui.font.xfont = nk_xfont_create(nkui.dpy, xftname);
    return &nkui.font;
}

NKUI_API void nkui_font_end(void) {
    if (!nkui.ctx) nkui_ensure_context_loaded();
}

#ifdef NKUI_IMAGE_LOADER
NKUI_API struct nk_image nkui_image_load_file(const char *filename) {
    return nk_xsurf_load_image_from_file(filename);
}

NKUI_API struct nk_image nkui_image_load_memory(const char *membuf, nk_uint memsize) {
    return nk_xsurf_load_image_from_memory(membuf, memsize);
}

NKUI_API void nkui_image_free(struct nk_image image) {
    nk_xsurf_image_free(&image);
}
#endif /* NKUI_IMAGE_LOADER */

#ifdef __cplusplus
}
#endif

#endif /* NKUI_IMPLEMENTATION */
#endif /* NKUI_XLIB_H */