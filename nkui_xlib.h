#ifndef NKUI_XLIB_H
#define NKUI_XLIB_H

#include "nkui.h"

#ifdef NKUI_IMPLEMENTATION

#define NK_XLIB_IMPLEMENTATION
#define NK_XLIB_USE_XFT

#if defined(NKUI_IMAGE_LOADER) && !defined(NKUI_NO_INCLUDE_STBIMAGE)
#ifndef NKUI_NO_STBIMAGE_IMPL
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <example/stb_image.h>
#endif /* NKUI_IMAGE_LOADED && !NKUI_NO_INCLUDE_STBIMAGE */

#include <time.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xft/Xft.h>

#ifndef NKUI_MINLOOP_DELAY
#define NKUI_MINLOOP_DELAY      20
#endif
#ifndef NKUI_DOUBLECLICK_LO
#define NKUI_DOUBLECLICK_LO     20
#endif
#ifndef NKUI_DOUBLECLICK_HI
#define NKUI_DOUBLECLICK_HI     200
#endif

#define ANGLE_NK_TO_X(a)        ((a) * 180 * 64 / NK_PI)
#define ANGLE_X_90(f)           ((f) * 90 * 64)

#ifdef __cplusplus
extern "C" {
#endif

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

    GC gc;
    Drawable dw;
    XftDraw *xft;

    struct atoms {
        Atom wm_protocols;
        Atom wm_delete_window;
        Atom xa_clipboard;
        Atom xa_targets;
        Atom xa_text;
        Atom xa_utf8_string;
    } atoms;

    long last_clicked;
    long loop_started;
    nk_bool stop;

    struct nk_context ctx;
    struct nk_user_font *fonts;
    int num_fonts;

    nkui_draw_fn draw;
    nkui_clear_fn clear;
    nkui_init_fn init;
    nkui_term_fn term;
    void *userdata;
} nkui;

struct nkui_user_image {
    XImage *ximage;
    GC gc;
    Pixmap mask;
};

static long nkui__timestamp(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) return 0;
    return (long)((long)tv.tv_sec * 1000 + (long)tv.tv_usec / 1000);
}

static void nkui__sleep_for(long t) {
    struct timespec ts;
    const time_t sec = (int)(t / 1000);
    const long ms = t - (sec * 1000);
    ts.tv_sec = sec;
    ts.tv_nsec = ms * 1000000L;
    while (-1 == nanosleep(&ts, &ts));
}

static unsigned long nkui__convert_color(struct nk_color c) {
    unsigned long ret = 0;
    ret |= (unsigned long)(c.r << 16);
    ret |= (unsigned long)(c.g << 8);
    ret |= (unsigned long)(c.b << 0);
    return ret;
}

/* Font management ***********************************************************/

static float nkui__font_get_text_width(nk_handle handle, float height, const char *text, int len) {
    XGlyphInfo gi;
    NK_UNUSED(height);
    if (!handle.ptr  || !text || *text == '\0') return 0;
    XftTextExtentsUtf8(nkui.dpy, handle.ptr, (const FcChar8 *)text, len, &gi);
    return gi.xOff;
}

static struct nk_user_font *nkui__font_add(XftFont *ft) {
    struct nk_user_font *font = NULL;
    int i;

    if (!ft) return NULL;
    for (i = 0; i < nkui.num_fonts; ++i) {
        if (!nkui.fonts[i].userdata.ptr) {
            /* reuse this empty index */
            font = nkui.fonts + i;
            break;
        }
    }
    if (!font) {
        nkui.fonts = NKUI_REALLOC(nkui.fonts, nkui.num_fonts + 1);
        font = nkui.fonts + nkui.num_fonts;
        ++nkui.num_fonts;
    }

    font->userdata.ptr = ft;
    font->height = ft->height;
    font->width = nkui__font_get_text_width;

    return font;
}

static void nkui__font_free(struct nk_user_font *font) {
    int i;
    for (i = 0; i < nkui.num_fonts; ++i) {
        if (font == &nkui.fonts[i]) {
            if (font->userdata.ptr) XftFontClose(nkui.dpy, font->userdata.ptr);
            NKUI_ZERO(font, sizeof(struct nk_user_font));
            break;
        }
    }
}


/* Render ********************************************************************/

static void nkui__draw_scissor(const struct nk_command_scissor *c) {
    XRectangle clip = {
        c->x - 1,
        c->y - 1,
        c->w + 2,
        c->h + 2
    };
    XSetClipRectangles(nkui.dpy, nkui.gc, 0, 0, &clip, 1, Unsorted);
}

static void nkui__draw_line(const struct nk_command_line *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->begin.x, c->begin.y, c->end.x, c->end.y);
}

static void nkui__draw_curve(const struct nk_command_curve *c) {
    struct nk_vec2i last = c->begin;
    unsigned int segments = NK_MAX(NKUI_CURVE_SEGMENTS, 1);
    unsigned int istep;
    float tstep = 1.0f / (float)segments;
    float t, u, w1, w2, w3, w4;
    short x, y;

    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    for (istep = 1; istep <= segments; ++istep) {
        t = tstep * (float)istep;
        u = 1.0f - t;
        w1 = 1 * u * u * u;
        w2 = 3 * u * u * t;
        w3 = 3 * u * t * t;
        w4 = 1 * t * t * t;
        x = (short)(w1 * c->begin.x + w2 * c->ctrl[0].x + w3 * c->ctrl[1].x + w4 * c->end.x);
        y = (short)(w1 * c->begin.y + w2 * c->ctrl[0].y + w3 * c->ctrl[1].y + w4 * c->end.y);
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, last.x, last.y, x, y);
        last.x = x; last.y = y;
    }
}

static void nkui__draw_rect(const struct nk_command_rect *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    if (!c->rounding) {
        XDrawRectangle(nkui.dpy, nkui.dw, nkui.gc, c->x, c->y, c->w, c->h);
    }
    else {
        short r = c->rounding;
        short xc = c->x + r;
        short yc = c->y + r;
        short wc = c->w - 2 * r;
        short hc = c->h - 2 * r;
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, xc, c->y, xc + wc, c->y);
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->x + c->w, yc, c->x + c->w, yc + hc);
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, xc, c->y + c->h, xc + wc, c->y + c->h);
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->x, yc, c->x, yc + hc);
        XDrawArc(nkui.dpy, nkui.dw, nkui.gc, xc + wc - r, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(0), ANGLE_X_90(1));
        XDrawArc(nkui.dpy, nkui.dw, nkui.gc, c->x, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(1), ANGLE_X_90(1));
        XDrawArc(nkui.dpy, nkui.dw, nkui.gc, c->x, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(2), ANGLE_X_90(1));
        XDrawArc(nkui.dpy, nkui.dw, nkui.gc, xc + wc - r, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(3), ANGLE_X_90(1));
    }
}

static void nkui__draw_rect_filled(const struct nk_command_rect_filled *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    if (!c->rounding) {
        XFillRectangle(nkui.dpy, nkui.dw, nkui.gc, c->x, c->y, c->w, c->h);
    }
    else {
        short r = c->rounding;
        short xc = c->x + r;
        short yc = c->y + r;
        short wc = c->w - 2 * r;
        short hc = c->h - 2 * r;
        XPoint pt[12] = {
            { c->x, yc },             { xc, yc },           { xc, c->y },
            { xc + wc, c->y },        { xc + wc, yc },      { c->x + c->w, yc },
            { c->x + c->w, yc + hc }, { xc + wc, yc + hc }, { xc + wc, c->y + c->h },
            { xc, c->y + c->h },      { xc, yc + hc },      { c->x, yc + hc }
        };

        XFillPolygon(nkui.dpy, nkui.dw, nkui.gc, pt, 12, Convex, CoordModeOrigin);
        XFillArc(nkui.dpy, nkui.dw, nkui.gc, xc + wc - r, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(0), ANGLE_X_90(1));
        XFillArc(nkui.dpy, nkui.dw, nkui.gc, c->x, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(1), ANGLE_X_90(1));
        XFillArc(nkui.dpy, nkui.dw, nkui.gc, c->x, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(2), ANGLE_X_90(1));
        XFillArc(nkui.dpy, nkui.dw, nkui.gc, xc + wc - r, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(3), ANGLE_X_90(1));
    }
}

static void nkui__draw_rect_multi_color(const struct nk_command_rect_multi_color *c) {
    NK_UNUSED(c);
}

static void nkui__draw_circle(const struct nk_command_circle *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawArc(nkui.dpy, nkui.dw, nkui.gc, c->x, c->y, c->w, c->h, 0, ANGLE_X_90(4));
}

static void nkui__draw_circle_filled(const struct nk_command_circle_filled *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XFillArc(nkui.dpy, nkui.dw, nkui.gc, c->x, c->y, c->w, c->h, 0, ANGLE_X_90(4));
}

static void nkui__draw_arc(const struct nk_command_arc *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawArc(nkui.dpy, nkui.dw, nkui.gc, (int)(c->cx - c->r), (int)(c->cy - c->r),
        (unsigned int)(c->r * 2), (unsigned int)(c->r * 2),
        (int)ANGLE_NK_TO_X(c->a[0]), (int)ANGLE_NK_TO_X(c->a[1]));

}

static void nkui__draw_arc_filled(const struct nk_command_arc_filled *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XFillArc(nkui.dpy, nkui.dw, nkui.gc, (int)(c->cx - c->r), (int)(c->cy - c->r),
        (unsigned int)(c->r * 2), (unsigned int)(c->r * 2),
        (int)ANGLE_NK_TO_X(c->a[0]), (int)ANGLE_NK_TO_X(c->a[1]));
}

static void nkui__draw_triangle(const struct nk_command_triangle *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->a.x, c->a.y, c->b.x, c->b.y);
    XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->b.x, c->b.y, c->c.x, c->c.y);
    XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->c.x, c->c.y, c->a.x, c->a.y);
}

static void nkui__draw_triangle_filled(const struct nk_command_triangle_filled *c) {
    XPoint pt[3] = {
        { c->a.x, c->a.y },
        { c->b.x, c->b.y },
        { c->c.x, c->c.y }
    };
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XFillPolygon(nkui.dpy, nkui.dw, nkui.gc, pt, 3, Convex, CoordModeOrigin);
}

static void nkui__draw_polygon(const struct nk_command_polygon *c) {
    int i, ct = c->point_count;
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    for (i = 1; i < ct; ++i) {
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->points[i-1].x, c->points[i-1].y,
            c->points[i].x, c->points[i].y);
    }
    XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->points[ct-1].x, c->points[ct-1].y,
        c->points[0].x, c->points[0].y);
}

static void nkui__draw_polygon_filled(const struct nk_command_polygon_filled *c) {
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XFillPolygon(nkui.dpy, nkui.dw, nkui.gc, (XPoint *)c->points, c->point_count, Convex, CoordModeOrigin);
}

static void nkui__draw_polyline(const struct nk_command_polyline *c) {
    int i;
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(c->color));
    XSetLineAttributes(nkui.dpy, nkui.gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    for (i = 0; i < c->point_count - 1; ++i) {
        XDrawLine(nkui.dpy, nkui.dw, nkui.gc, c->points[i].x, c->points[i].y,
            c->points[i+1].x, c->points[i+1].y);
    }
}

static void nkui__draw_text(const struct nk_command_text *c) {
    XftColor color;
    XRenderColor xrc = {
        c->foreground.r * 257,
        c->foreground.g * 257,
        c->foreground.b * 257,
        c->foreground.a * 257
    };
    XftFont *font = c->font ? c->font->userdata.ptr : NULL;

    if (!font || !c->length) return;
    XftColorAllocValue(nkui.dpy, nkui.vis, nkui.cmap, &xrc, &color);
    XftDrawStringUtf8(nkui.xft, &color, font, c->x, c->y + font->ascent, (const FcChar8 *)c->string, c->length);
    XftColorFree(nkui.dpy, nkui.vis, nkui.cmap, &color);
}

static void nkui__draw_image(const struct nk_command_image *c) {
    struct nkui_user_image *uimage = c->img.handle.ptr;
    if (uimage) {
        if (uimage->mask) {
            XSetClipMask(nkui.dpy, nkui.gc, uimage->mask);
            XSetClipOrigin(nkui.dpy, nkui.gc, c->x, c->y);
        }
        XPutImage(nkui.dpy, nkui.dw, nkui.gc, uimage->ximage, 0, 0,
            c->x, c->y, c->w, c->h);
        XSetClipMask(nkui.dpy, nkui.gc, None);
    }
}

static void nkui__draw_custom(const struct nk_command_custom *c) {
    NK_UNUSED(c);
}


static struct nk_image nkui__convert_stbi_image(unsigned char *data, int w, int h, int channels) {
    struct nk_image img;
    struct nkui_user_image *uimage;
    int bpl = channels;
    int isize = w * h * channels;
    int i;

    if (!data) return nk_image_id(0);
    uimage = calloc(1, sizeof(struct nkui_user_image));
    if (!uimage) return nk_image_id(0);

    switch (nkui.depth) {
    case 24:            bpl = 4; break;
    case 16: case 15:   bpl = 2; break;
    default:            bpl = 1; break;
    }

    /* rgba to bgra */
    if (channels >= 3) {
        unsigned char red, blue;
        for (i = 0; i < isize; i += channels) {
            /* swap red and blue */
            red = data[i+2];
            blue = data[i];
            data[i] = red;
            data[i+2] = blue;
        }
    }
    if (channels == 4) {
        static const unsigned alpha_treshold=  127;
        uimage->mask = XCreatePixmap(nkui.dpy, nkui.dw, w, h, 1);
        if (uimage->mask) {
            unsigned char alpha;
            int div, x, y;
            uimage->gc = XCreateGC(nkui.dpy, uimage->mask, 0, NULL);
            XSetForeground(nkui.dpy, uimage->gc, WhitePixel(nkui.dpy, nkui.screen));
            XFillRectangle(nkui.dpy, uimage->mask, uimage->gc, 0, 0, w, h);
            XSetForeground(nkui.dpy, uimage->gc, BlackPixel(nkui.dpy, nkui.screen));
            for (i = 0; i < isize; i += channels) {
                alpha = data[i+3];
                div = i / channels;
                x = div % w;
                y = div / w;
                if (alpha > alpha_treshold) {
                    XDrawPoint(nkui.dpy, uimage->mask, uimage->gc, x, y);
                }
            }
        }
    }

    uimage->ximage = XCreateImage(nkui.dpy, CopyFromParent, nkui.depth,
        ZPixmap, 0, (char *)data, w, h, bpl * 8, bpl * w);
    img.handle.ptr = (void *)uimage;
    img.w = w;
    img.h = h;
    return img;
}


/* Events ********************************************************************/

static void nkui__ensure_font_loaded(void) {
    if (!nkui.fonts) {
        nkui_font_load_native(NKUI_DEFAULT_FONT_NATIVE, NKUI_DEFAULT_FONT_SIZE);
        nk_style_set_font(&nkui.ctx, nkui.fonts);
    }
}


/* API ***********************************************************************/

NKUI_API nk_bool nkui_init(struct nkui_params *params) {
    XSetWindowAttributes xswa;
    int width; int height;
    const char *title;

    NK_ASSERT(params);

    NKUI_ZERO(&nkui, sizeof(nkui));
    nkui.dpy = XOpenDisplay(NULL);
    if (!nkui.dpy) return nk_false;
    nkui.screen = XDefaultScreen(nkui.dpy);
    nkui.depth = DefaultDepth(nkui.dpy, nkui.screen);
    nkui.root = RootWindow(nkui.dpy, nkui.screen);
    nkui.vis = DefaultVisual(nkui.dpy, nkui.screen);
    nkui.cmap = DefaultColormap(nkui.dpy, nkui.screen);

    nkui.atoms.wm_protocols = XInternAtom(nkui.dpy, "WM_PROTOCOLS", False);
    nkui.atoms.wm_delete_window = XInternAtom(nkui.dpy, "WM_DELETE_WINDOW", False);
    nkui.atoms.xa_clipboard = XInternAtom(nkui.dpy, "CLIPBOARD", False);
    nkui.atoms.xa_targets = XInternAtom(nkui.dpy, "TARGETS", False);
    nkui.atoms.xa_text = XInternAtom(nkui.dpy, "TEXT", False);
    nkui.atoms.xa_utf8_string = XInternAtom(nkui.dpy, "UTF8_STRING", False);

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
    XSetWMProtocols(nkui.dpy, nkui.xwin, &nkui.atoms.wm_delete_window, 1);
    XGetWindowAttributes(nkui.dpy, nkui.xwin, &nkui.xwa);

    {   /* invisible cursor (for mouse grabbing) */
        static XColor dummy;
        static char d[1] = {0};
        Pixmap blank = XCreateBitmapFromData(nkui.dpy, nkui.xwin, d, 1, 1);
        nkui.hidden_cursor = XCreatePixmapCursor(nkui.dpy, blank, blank, &dummy, &dummy, 0, 0);
        XFreePixmap(nkui.dpy, blank);
    }

    /* create the back pixmap to draw on (and associated Xft surface) */
    nkui.gc = XCreateGC(nkui.dpy, nkui.xwin, 0, NULL);
    nkui.dw = XCreatePixmap(nkui.dpy, nkui.xwin, nkui.xwa.width, nkui.xwa.height, nkui.depth);
    nkui.xft = XftDrawCreate(nkui.dpy, nkui.dw, nkui.vis, nkui.cmap);

    /* initialize nuklear context */
    if (!nk_init_default(&nkui.ctx, NULL)) {
        nkui_shutdown();
        return nk_false;
    }

    if (nkui.init) {
        if (!nkui.init(&nkui.ctx, nkui.userdata)) {
            /* something went wrong, cleanup everything */
            nkui_shutdown();
            return nk_false;
        }
    }

    /* create an initial set of 2 font slots */
    nkui.fonts = NKUI_REALLOC(NULL, 2 * sizeof(struct nk_user_font));
    NKUI_ZERO(nkui.fonts, 2 * sizeof(struct nk_user_font));

    /* XSynchronize(nkui.dpy, True); */

    return nk_true;
}

NKUI_API void nkui_shutdown(void) {
    int i;

    XUnmapWindow(nkui.dpy, nkui.xwin);
    XDestroyWindow(nkui.dpy, nkui.xwin);

    nk_free(&nkui.ctx);
    if (nkui.fonts) {
        for (i = 0; i < nkui.num_fonts; ++i) {
            if (nkui.fonts[i].userdata.ptr) XftFontClose(nkui.dpy, nkui.fonts[i].userdata.ptr);
        }
        NKUI_FREE(nkui.fonts);
    }
    XftDrawDestroy(nkui.xft);
    XFreePixmap(nkui.dpy, nkui.dw);
    XFreeGC(nkui.dpy, nkui.gc);
    XFreeCursor(nkui.dpy, nkui.hidden_cursor);
    XCloseDisplay(nkui.dpy);
}

NKUI_API nk_bool nkui_events(int wait) {
    nk_bool ret = nk_true;
    XEvent event;
    struct nk_context *ctx = &nkui.ctx;

    NK_UNUSED(wait);
    if (nkui.stop) return nk_false;

    /* if no context, then no font loaded, ensure all is loaded now */
    nkui__ensure_font_loaded();

    nkui.loop_started = nkui__timestamp();
    nk_input_begin(&nkui.ctx);
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

        /* mouse grabbing */
        if (ctx->input.mouse.grab) {
            XDefineCursor(nkui.dpy, nkui.xwin, nkui.hidden_cursor);
            ctx->input.mouse.grab = 0;
        }
        else if (ctx->input.mouse.ungrab) {
            XWarpPointer(nkui.dpy, None, nkui.xwin, 0, 0, 0, 0,
                (int)ctx->input.mouse.prev.x, (int)ctx->input.mouse.prev.y);
            XUndefineCursor(nkui.dpy, nkui.xwin);
            ctx->input.mouse.ungrab = 0;
        }

        switch (event.type) {
        case KeyPress:
        case KeyRelease: {
            nk_bool down = (event.type == KeyPress);
            int ret;
            KeySym *code = XGetKeyboardMapping(nkui.dpy, (KeyCode)event.xkey.keycode, 1, &ret);
            switch (*code) {
            case XK_Shift_L:
            case XK_Shift_R:    nk_input_key(ctx, NK_KEY_SHIFT, down); break;
            case XK_Delete:     nk_input_key(ctx, NK_KEY_DEL, down); break;
            case XK_Return:     nk_input_key(ctx, NK_KEY_ENTER, down); break;
            case XK_Tab:        nk_input_key(ctx, NK_KEY_TAB, down); break;
            case XK_BackSpace:  nk_input_key(ctx, NK_KEY_BACKSPACE, down); break;
            case XK_Home:       nk_input_key(ctx, NK_KEY_TEXT_START, down);
                                nk_input_key(ctx, NK_KEY_SCROLL_START, down); break;
            case XK_End:        nk_input_key(ctx, NK_KEY_TEXT_END, down);
                                nk_input_key(ctx, NK_KEY_SCROLL_END, down); break;
            case XK_Page_Down:  nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down); break;
            case XK_Page_Up:    nk_input_key(ctx, NK_KEY_SCROLL_UP, down); break;
            case XK_Down:       nk_input_key(ctx, NK_KEY_DOWN, down); break;
            case XK_Up:         nk_input_key(ctx, NK_KEY_UP, down); break;
            default:
                if (event.xkey.state & ControlMask) {
                    switch (*code) {
                    case XK_z:      nk_input_key(ctx, NK_KEY_TEXT_UNDO, down); break;
                    case XK_y:      nk_input_key(ctx, NK_KEY_TEXT_REDO, down); break;
                    case XK_c:      nk_input_key(ctx, NK_KEY_COPY, down); break;
                    case XK_v:      nk_input_key(ctx, NK_KEY_PASTE, down); break;
                    case XK_x:      nk_input_key(ctx, NK_KEY_CUT, down); break;
                    case XK_b:      nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down); break;
                    case XK_e:      nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down); break;
                    }
                }
                else {
                    switch (*code) {
                    case XK_Left:   nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down); break;
                    case XK_Right:  nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down); break;
                    default:
                        if (down) {
                            char buf[32];
                            KeySym keysym = 0;
                            if (XLookupString(&event.xkey, buf, 32, &keysym, NULL) != NoSymbol) {
                                nk_input_glyph(ctx, buf);
                            }
                        }                    
                    }
                }
            }
            XFree(code);
        } break;
        case ButtonPress:
        case ButtonRelease: {
            nk_bool down = (event.type == ButtonPress);
            int x = event.xbutton.x, y = event.xbutton.y;
            switch (event.xbutton.button) {
            case Button1:
                if (down) {
                    long dt = nkui__timestamp() - nkui.last_clicked;
                    if (dt > NKUI_DOUBLECLICK_LO && dt < NKUI_DOUBLECLICK_HI) {
                        nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, nk_true);
                    }
                }
                else {
                    nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, nk_false);
                }
                nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
                break;
            case Button2:   nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down); break;
            case Button3:   nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down); break;
            case Button4:   nk_input_scroll(ctx, nk_vec2(0, 1.0f)); break;
            case Button5:   nk_input_scroll(ctx, nk_vec2(0, -1.0f)); break;
            }
        } break;
        case MotionNotify: {
            nk_input_motion(&nkui.ctx, event.xmotion.x, event.xmotion.y);
            if (ctx->input.mouse.grabbed) {
                ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
                ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
                XWarpPointer(nkui.dpy, None, nkui.xwin, 0, 0, 0, 0,
                    (int)ctx->input.mouse.pos.x, (int)ctx->input.mouse.pos.y);
            }
        } break;
        case Expose:
        case ConfigureNotify: {
            XGetWindowAttributes(nkui.dpy, nkui.xwin, &nkui.xwa);
            /* if (event.type == ConfigureNotify) { */
                XFreePixmap(nkui.dpy, nkui.dw);
                nkui.dw = XCreatePixmap(nkui.dpy, nkui.xwin, nkui.xwa.width, nkui.xwa.height, nkui.depth);
                XftDrawChange(nkui.xft, nkui.dw);
            /* } */
        } break;
        case KeymapNotify: {
            XRefreshKeyboardMapping(&event.xmapping);
        } break;
        case SelectionRequest:
        case SelectionNotify: {
            /* TODO */
        } break;
        }
    }
    nk_input_end(&nkui.ctx);
    return ret;
}

NKUI_API void nkui_stop(void) {
    nkui.stop = nk_true;
}

NKUI_API void nkui_render(void) {
    long dt;
    const struct nk_command *cmd;

    XGetWindowAttributes(nkui.dpy, nkui.xwin, &nkui.xwa);
    nkui.draw(&nkui.ctx, nkui.xwa.width, nkui.xwa.height, nkui.userdata);
    if (nkui.stop) return;

    /* XClearWindow(nkui.dpy, nkui.xwin); */
    XSetForeground(nkui.dpy, nkui.gc, nkui__convert_color(nkui.clear(nkui.userdata)));
    XFillRectangle(nkui.dpy, nkui.dw, nkui.gc, 0, 0, nkui.xwa.width, nkui.xwa.height);

    nk_foreach(cmd, &nkui.ctx) {
        switch (cmd->type) {
        case NK_COMMAND_NOP:                break;
        case NK_COMMAND_SCISSOR:            nkui__draw_scissor((const struct nk_command_scissor *)cmd); break;
        case NK_COMMAND_LINE:               nkui__draw_line((const struct nk_command_line *)cmd); break;
        case NK_COMMAND_CURVE:              nkui__draw_curve((const struct nk_command_curve *)cmd); break;
        case NK_COMMAND_RECT:               nkui__draw_rect((const struct nk_command_rect *)cmd); break;
        case NK_COMMAND_RECT_FILLED:        nkui__draw_rect_filled((const struct nk_command_rect_filled *)cmd); break;
        case NK_COMMAND_RECT_MULTI_COLOR:   nkui__draw_rect_multi_color((const struct nk_command_rect_multi_color *)cmd); break;
        case NK_COMMAND_CIRCLE:             nkui__draw_circle((const struct nk_command_circle *)cmd); break;
        case NK_COMMAND_CIRCLE_FILLED:      nkui__draw_circle_filled((const struct nk_command_circle_filled *)cmd); break;
        case NK_COMMAND_ARC:                nkui__draw_arc((const struct nk_command_arc *)cmd); break;
        case NK_COMMAND_ARC_FILLED:         nkui__draw_arc_filled((const struct nk_command_arc_filled *)cmd); break;
        case NK_COMMAND_TRIANGLE:           nkui__draw_triangle((const struct nk_command_triangle *)cmd); break;
        case NK_COMMAND_TRIANGLE_FILLED:    nkui__draw_triangle_filled((const struct nk_command_triangle_filled *)cmd); break;
        case NK_COMMAND_POLYGON:            nkui__draw_polygon((const struct nk_command_polygon *)cmd); break;
        case NK_COMMAND_POLYGON_FILLED:     nkui__draw_polygon_filled((const struct nk_command_polygon_filled *)cmd); break;
        case NK_COMMAND_POLYLINE:           nkui__draw_polyline((const struct nk_command_polyline *)cmd); break;
        case NK_COMMAND_TEXT:               nkui__draw_text((const struct nk_command_text *)cmd); break;
        case NK_COMMAND_IMAGE:              nkui__draw_image((const struct nk_command_image *)cmd); break;
        case NK_COMMAND_CUSTOM:             nkui__draw_custom((const struct nk_command_custom *)cmd); break;
        }
    }
    nk_clear(&nkui.ctx);

    XCopyArea(nkui.dpy, nkui.dw, nkui.xwin, nkui.gc, 0, 0, nkui.xwa.width, nkui.xwa.height, 0, 0);
    XFlush(nkui.dpy);

    dt = nkui__timestamp() - nkui.loop_started;
    if (dt < NKUI_MINLOOP_DELAY) nkui__sleep_for(NKUI_MINLOOP_DELAY - dt);
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
NKUI_API struct nk_user_font *nkui_font_load_file(const char *name, int size) {
    NK_UNUSED(name);
    NK_UNUSED(size);
    return NULL;
}
#endif

NKUI_API struct nk_user_font *nkui_font_load_native(const char *name, int size) {
    char xftname[256];
    XftFont *xft;

    snprintf(xftname, 256, "%s-%d", name, size);
    xft = XftFontOpenName(nkui.dpy, nkui.screen, xftname);
    if (!xft) return NULL;
    return nkui__font_add(xft);

}

NKUI_API void nkui_font_end(void) {
    nkui__ensure_font_loaded();
}

NKUI_API void nkui_font_free(struct nk_user_font *font) {
    nkui__font_free(font);
}

#ifdef NKUI_IMAGE_LOADER
NKUI_API struct nk_image nkui_image_load_file(const char *filename) {
    int w, h, channels;
    unsigned char *data = stbi_load(filename, &w, &h, &channels, 0);
    return nkui__convert_stbi_image(data, w, h, channels);
}

NKUI_API struct nk_image nkui_image_load_memory(const unsigned char *membuf, nk_uint memsize) {
    int w, h, channels;
    unsigned char *data = stbi_load_from_memory(membuf, memsize, &w, &h, &channels, 0);
    return nkui__convert_stbi_image(data, w, h, channels);
}

NKUI_API void nkui_image_free(struct nk_image image) {
    struct nkui_user_image *uimage = image.handle.ptr;
    if (uimage) XDestroyImage(uimage->ximage);
    XFreePixmap(nkui.dpy, uimage->mask);
    XFreeGC(nkui.dpy, uimage->gc);
    free(uimage);
}
#endif /* NKUI_IMAGE_LOADER */

#ifdef __cplusplus
}
#endif

#endif /* NKUI_IMPLEMENTATION */
#endif /* NKUI_XLIB_H */