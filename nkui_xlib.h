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

struct nkui_font {
    struct nkui *ui;
    struct nk_user_font nkuf;
    XftFont *xft;
};

struct nkui_image {
    XImage *ximage;
    GC gc;
    Pixmap mask;
};

struct nkui {
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
    //struct nk_user_font *fonts;
    struct nkui_font *fonts;
    int num_fonts;

    nkui_draw_fn draw;
    nkui_clear_fn clear;
    nkui_init_fn init;
    nkui_term_fn term;
    void *userdata;
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
    struct nkui_font *ft;
    NK_UNUSED(height);
    if (!handle.ptr  || !text || *text == '\0') return 0;
    ft = (struct nkui_font *)handle.ptr;
    XftTextExtentsUtf8(ft->ui->dpy, ft->xft, (const FcChar8 *)text, len, &gi);
    return gi.xOff;
}

static struct nk_user_font *nkui__font_add(struct nkui *ui, XftFont *ft) {
    struct nkui_font *font = NULL;
    //struct nk_user_font *font = NULL;
    int i;

    if (!ft) return NULL;
    for (i = 0; i < ui->num_fonts; ++i) {
        if (!ui->fonts[i].ui) {
        //if (!ui->fonts[i].userdata.ptr) {
            /* reuse this empty index */
            font = ui->fonts + i;
            break;
        }
    }
    if (!font) {
        ui->fonts = NKUI_REALLOC(ui->fonts, ui->num_fonts + 1);
        font = ui->fonts + ui->num_fonts;
        ++ui->num_fonts;
    }

    font->ui = ui;
    font->xft = ft;
    font->nkuf.height = ft->height;
    font->nkuf.userdata.ptr = font;
    font->nkuf.width = nkui__font_get_text_width;
    //font->userdata.ptr = ft;
    //font->height = ft->height;
    //font->width = nkui__font_get_text_width;

    return &font->nkuf;
}

static void nkui__font_free(struct nkui *ui, struct nk_user_font *nkuf) {
    struct nkui_font *font = nkuf->userdata.ptr;
    int i;

    for (i = 0; i < ui->num_fonts; ++i) {
        if (font == ui->fonts + i) {
            if (font->xft) XftFontClose(ui->dpy, font->xft);
            NKUI_ZERO(font, sizeof(struct nkui_font));
            break;
        }
    }
}


/* Render ********************************************************************/

static void nkui__draw_scissor(struct nkui *ui, const struct nk_command_scissor *c) {
    XRectangle clip = {
        c->x - 1,
        c->y - 1,
        c->w + 2,
        c->h + 2
    };
    XSetClipRectangles(ui->dpy, ui->gc, 0, 0, &clip, 1, Unsorted);
}

static void nkui__draw_line(struct nkui *ui, const struct nk_command_line *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawLine(ui->dpy, ui->dw, ui->gc, c->begin.x, c->begin.y, c->end.x, c->end.y);
}

static void nkui__draw_curve(struct nkui *ui, const struct nk_command_curve *c) {
    struct nk_vec2i last = c->begin;
    unsigned int segments = NK_MAX(NKUI_CURVE_SEGMENTS, 1);
    unsigned int istep;
    float tstep = 1.0f / (float)segments;
    float t, u, w1, w2, w3, w4;
    short x, y;

    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    for (istep = 1; istep <= segments; ++istep) {
        t = tstep * (float)istep;
        u = 1.0f - t;
        w1 = 1 * u * u * u;
        w2 = 3 * u * u * t;
        w3 = 3 * u * t * t;
        w4 = 1 * t * t * t;
        x = (short)(w1 * c->begin.x + w2 * c->ctrl[0].x + w3 * c->ctrl[1].x + w4 * c->end.x);
        y = (short)(w1 * c->begin.y + w2 * c->ctrl[0].y + w3 * c->ctrl[1].y + w4 * c->end.y);
        XDrawLine(ui->dpy, ui->dw, ui->gc, last.x, last.y, x, y);
        last.x = x; last.y = y;
    }
}

static void nkui__draw_rect(struct nkui *ui, const struct nk_command_rect *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    if (!c->rounding) {
        XDrawRectangle(ui->dpy, ui->dw, ui->gc, c->x, c->y, c->w, c->h);
    }
    else {
        short r = c->rounding;
        short xc = c->x + r;
        short yc = c->y + r;
        short wc = c->w - 2 * r;
        short hc = c->h - 2 * r;
        XDrawLine(ui->dpy, ui->dw, ui->gc, xc, c->y, xc + wc, c->y);
        XDrawLine(ui->dpy, ui->dw, ui->gc, c->x + c->w, yc, c->x + c->w, yc + hc);
        XDrawLine(ui->dpy, ui->dw, ui->gc, xc, c->y + c->h, xc + wc, c->y + c->h);
        XDrawLine(ui->dpy, ui->dw, ui->gc, c->x, yc, c->x, yc + hc);
        XDrawArc(ui->dpy, ui->dw, ui->gc, xc + wc - r, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(0), ANGLE_X_90(1));
        XDrawArc(ui->dpy, ui->dw, ui->gc, c->x, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(1), ANGLE_X_90(1));
        XDrawArc(ui->dpy, ui->dw, ui->gc, c->x, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(2), ANGLE_X_90(1));
        XDrawArc(ui->dpy, ui->dw, ui->gc, xc + wc - r, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(3), ANGLE_X_90(1));
    }
}

static void nkui__draw_rect_filled(struct nkui *ui, const struct nk_command_rect_filled *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    if (!c->rounding) {
        XFillRectangle(ui->dpy, ui->dw, ui->gc, c->x, c->y, c->w, c->h);
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

        XFillPolygon(ui->dpy, ui->dw, ui->gc, pt, 12, Convex, CoordModeOrigin);
        XFillArc(ui->dpy, ui->dw, ui->gc, xc + wc - r, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(0), ANGLE_X_90(1));
        XFillArc(ui->dpy, ui->dw, ui->gc, c->x, c->y,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(1), ANGLE_X_90(1));
        XFillArc(ui->dpy, ui->dw, ui->gc, c->x, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(2), ANGLE_X_90(1));
        XFillArc(ui->dpy, ui->dw, ui->gc, xc + wc - r, yc + hc - r,
            (unsigned int)r*2, (unsigned int)r * 2, ANGLE_X_90(3), ANGLE_X_90(1));
    }
}

static void nkui__draw_rect_multi_color(struct nkui *ui, const struct nk_command_rect_multi_color *c) {
    NK_UNUSED(ui);
    NK_UNUSED(c);
}

static void nkui__draw_circle(struct nkui *ui, const struct nk_command_circle *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawArc(ui->dpy, ui->dw, ui->gc, c->x, c->y, c->w, c->h, 0, ANGLE_X_90(4));
}

static void nkui__draw_circle_filled(struct nkui *ui, const struct nk_command_circle_filled *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XFillArc(ui->dpy, ui->dw, ui->gc, c->x, c->y, c->w, c->h, 0, ANGLE_X_90(4));
}

static void nkui__draw_arc(struct nkui *ui, const struct nk_command_arc *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawArc(ui->dpy, ui->dw, ui->gc, (int)(c->cx - c->r), (int)(c->cy - c->r),
        (unsigned int)(c->r * 2), (unsigned int)(c->r * 2),
        (int)ANGLE_NK_TO_X(c->a[0]), (int)ANGLE_NK_TO_X(c->a[1]));

}

static void nkui__draw_arc_filled(struct nkui *ui, const struct nk_command_arc_filled *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XFillArc(ui->dpy, ui->dw, ui->gc, (int)(c->cx - c->r), (int)(c->cy - c->r),
        (unsigned int)(c->r * 2), (unsigned int)(c->r * 2),
        (int)ANGLE_NK_TO_X(c->a[0]), (int)ANGLE_NK_TO_X(c->a[1]));
}

static void nkui__draw_triangle(struct nkui *ui, const struct nk_command_triangle *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    XDrawLine(ui->dpy, ui->dw, ui->gc, c->a.x, c->a.y, c->b.x, c->b.y);
    XDrawLine(ui->dpy, ui->dw, ui->gc, c->b.x, c->b.y, c->c.x, c->c.y);
    XDrawLine(ui->dpy, ui->dw, ui->gc, c->c.x, c->c.y, c->a.x, c->a.y);
}

static void nkui__draw_triangle_filled(struct nkui *ui, const struct nk_command_triangle_filled *c) {
    XPoint pt[3] = {
        { c->a.x, c->a.y },
        { c->b.x, c->b.y },
        { c->c.x, c->c.y }
    };
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XFillPolygon(ui->dpy, ui->dw, ui->gc, pt, 3, Convex, CoordModeOrigin);
}

static void nkui__draw_polygon(struct nkui *ui, const struct nk_command_polygon *c) {
    int i, ct = c->point_count;
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    for (i = 1; i < ct; ++i) {
        XDrawLine(ui->dpy, ui->dw, ui->gc, c->points[i-1].x, c->points[i-1].y,
            c->points[i].x, c->points[i].y);
    }
    XDrawLine(ui->dpy, ui->dw, ui->gc, c->points[ct-1].x, c->points[ct-1].y,
        c->points[0].x, c->points[0].y);
}

static void nkui__draw_polygon_filled(struct nkui *ui, const struct nk_command_polygon_filled *c) {
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XFillPolygon(ui->dpy, ui->dw, ui->gc, (XPoint *)c->points, c->point_count, Convex, CoordModeOrigin);
}

static void nkui__draw_polyline(struct nkui *ui, const struct nk_command_polyline *c) {
    int i;
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(c->color));
    XSetLineAttributes(ui->dpy, ui->gc, c->line_thickness, LineSolid, CapButt, JoinMiter);
    for (i = 0; i < c->point_count - 1; ++i) {
        XDrawLine(ui->dpy, ui->dw, ui->gc, c->points[i].x, c->points[i].y,
            c->points[i+1].x, c->points[i+1].y);
    }
}

static void nkui__draw_text(struct nkui *ui, const struct nk_command_text *c) {
    XftColor color;
    XRenderColor xrc = {
        c->foreground.r * 257,
        c->foreground.g * 257,
        c->foreground.b * 257,
        c->foreground.a * 257
    };
    struct nkui_font *font = c->font->userdata.ptr;
    XftFont *ft = font ? font->xft : NULL;

    if (!ft || !c->length) return;
    XftColorAllocValue(ui->dpy, ui->vis, ui->cmap, &xrc, &color);
    XftDrawStringUtf8(ui->xft, &color, ft, c->x, c->y + ft->ascent, (const FcChar8 *)c->string, c->length);
    XftColorFree(ui->dpy, ui->vis, ui->cmap, &color);
}

static void nkui__draw_image(struct nkui *ui, const struct nk_command_image *c) {
    struct nkui_image *uimage = c->img.handle.ptr;
    if (uimage) {
        if (uimage->mask) {
            XSetClipMask(ui->dpy, ui->gc, uimage->mask);
            XSetClipOrigin(ui->dpy, ui->gc, c->x, c->y);
        }
        XPutImage(ui->dpy, ui->dw, ui->gc, uimage->ximage, 0, 0,
            c->x, c->y, c->w, c->h);
        XSetClipMask(ui->dpy, ui->gc, None);
    }
}

static void nkui__draw_custom(struct nkui *ui, const struct nk_command_custom *c) {
    NK_UNUSED(ui);
    NK_UNUSED(c);
}


static struct nk_image nkui__convert_stbi_image(struct nkui *ui, unsigned char *data, int w, int h, int channels) {
    struct nk_image img;
    struct nkui_image *uimage;
    int bpl = channels;
    int isize = w * h * channels;
    int i;

    if (!data) return nk_image_id(0);
    uimage = calloc(1, sizeof(struct nkui_image));
    if (!uimage) return nk_image_id(0);

    switch (ui->depth) {
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
        uimage->mask = XCreatePixmap(ui->dpy, ui->dw, w, h, 1);
        if (uimage->mask) {
            unsigned char alpha;
            int div, x, y;
            uimage->gc = XCreateGC(ui->dpy, uimage->mask, 0, NULL);
            XSetForeground(ui->dpy, uimage->gc, WhitePixel(ui->dpy, ui->screen));
            XFillRectangle(ui->dpy, uimage->mask, uimage->gc, 0, 0, w, h);
            XSetForeground(ui->dpy, uimage->gc, BlackPixel(ui->dpy, ui->screen));
            for (i = 0; i < isize; i += channels) {
                alpha = data[i+3];
                div = i / channels;
                x = div % w;
                y = div / w;
                if (alpha > alpha_treshold) {
                    XDrawPoint(ui->dpy, uimage->mask, uimage->gc, x, y);
                }
            }
        }
    }

    uimage->ximage = XCreateImage(ui->dpy, CopyFromParent, ui->depth,
        ZPixmap, 0, (char *)data, w, h, bpl * 8, bpl * w);
    img.handle.ptr = (void *)uimage;
    img.w = w;
    img.h = h;
    return img;
}


/* Events ********************************************************************/

static void nkui__ensure_font_loaded(struct nkui *ui) {
    if (!ui->fonts || !ui->fonts->ui) {
        nkui_font_load_native(ui, NKUI_DEFAULT_FONT_NATIVE, NKUI_DEFAULT_FONT_SIZE);
        nk_style_set_font(&ui->ctx, &ui->fonts->nkuf);
    }
}


/* API ***********************************************************************/

NKUI_API struct nkui *nkui_init(struct nkui_params *params) {
    struct nkui *ui;
    XSetWindowAttributes xswa;
    int width; int height;
    const char *title;

    NK_ASSERT(params);

    ui = calloc(1, sizeof(struct nkui));
    /*NKUI_ZERO(&nkui, sizeof(nkui));*/
    ui->dpy = XOpenDisplay(NULL);
    if (!ui->dpy) return NULL;
    ui->screen = XDefaultScreen(ui->dpy);
    ui->depth = DefaultDepth(ui->dpy, ui->screen);
    ui->root = RootWindow(ui->dpy, ui->screen);
    ui->vis = DefaultVisual(ui->dpy, ui->screen);
    ui->cmap = DefaultColormap(ui->dpy, ui->screen);

    ui->atoms.wm_protocols = XInternAtom(ui->dpy, "WM_PROTOCOLS", False);
    ui->atoms.wm_delete_window = XInternAtom(ui->dpy, "WM_DELETE_WINDOW", False);
    ui->atoms.xa_clipboard = XInternAtom(ui->dpy, "CLIPBOARD", False);
    ui->atoms.xa_targets = XInternAtom(ui->dpy, "TARGETS", False);
    ui->atoms.xa_text = XInternAtom(ui->dpy, "TEXT", False);
    ui->atoms.xa_utf8_string = XInternAtom(ui->dpy, "UTF8_STRING", False);

    ui->draw = params->draw;
    ui->clear = params->clear ? params->clear : nkui_default_render_color;
    ui->init = params->init;
    ui->term = params->term;
    ui->userdata = params->userdata;

    width = params->width > 0 ? params->width : NKUI_DEFAULT_WIDTH;
    height = params->height > 0 ? params->height : NKUI_DEFAULT_HEIGHT;
    title = params->title ? params->title : NKUI_DEFAULT_TITLE;
    xswa.event_mask =   ExposureMask | StructureNotifyMask
                    |   KeyPressMask | KeyReleaseMask | KeymapStateMask
                    |   ButtonPressMask | ButtonReleaseMask | ButtonMotionMask
                    |   PointerMotionMask
                    ;
    ui->xwin = XCreateWindow(ui->dpy, ui->root, params->x, params->y, width, height,
        0, ui->depth, InputOutput, ui->vis, CWEventMask, &xswa);
    XStoreName(ui->dpy, ui->xwin, title);
    XMapWindow(ui->dpy, ui->xwin);
    XSetWMProtocols(ui->dpy, ui->xwin, &ui->atoms.wm_delete_window, 1);
    XGetWindowAttributes(ui->dpy, ui->xwin, &ui->xwa);

    {   /* invisible cursor (for mouse grabbing) */
        static XColor dummy;
        static char d[1] = {0};
        Pixmap blank = XCreateBitmapFromData(ui->dpy, ui->xwin, d, 1, 1);
        ui->hidden_cursor = XCreatePixmapCursor(ui->dpy, blank, blank, &dummy, &dummy, 0, 0);
        XFreePixmap(ui->dpy, blank);
    }

    /* create the back pixmap to draw on (and associated Xft surface) */
    ui->gc = XCreateGC(ui->dpy, ui->xwin, 0, NULL);
    ui->dw = XCreatePixmap(ui->dpy, ui->xwin, ui->xwa.width, ui->xwa.height, ui->depth);
    ui->xft = XftDrawCreate(ui->dpy, ui->dw, ui->vis, ui->cmap);

    /* create an initial set of 2 font slots */
/*    ui->fonts = NKUI_REALLOC(NULL, 2 * sizeof(struct nkui_font));
    NKUI_ZERO(ui->fonts, 2 * sizeof(struct nkui_font));*/

    /* initialize nuklear context */
    if (!nk_init_default(&ui->ctx, NULL)) {
        nkui_shutdown(ui);
        return NULL;
    }

    if (ui->init) {
        if (!ui->init(ui, &ui->ctx, ui->userdata)) {
            /* something went wrong, cleanup everything */
            nkui_shutdown(ui);
            return NULL;
        }
    }

    /* XSynchronize(ui->dpy, True); */

    return ui;
}

NKUI_API void nkui_shutdown(struct nkui *ui) {
    int i;

    if (ui->term) ui->term(ui, ui->userdata);

    XUnmapWindow(ui->dpy, ui->xwin);
    XDestroyWindow(ui->dpy, ui->xwin);

    nk_free(&ui->ctx);
    if (ui->fonts) {
        for (i = 0; i < ui->num_fonts; ++i) {
            if (ui->fonts[i].xft) XftFontClose(ui->dpy, ui->fonts[i].xft);
        }
        NKUI_FREE(ui->fonts);
    }
    XftDrawDestroy(ui->xft);
    XFreePixmap(ui->dpy, ui->dw);
    XFreeGC(ui->dpy, ui->gc);
    XFreeCursor(ui->dpy, ui->hidden_cursor);
    XCloseDisplay(ui->dpy);
    free(ui);
}

NKUI_API nk_bool nkui_events(struct nkui *ui, int wait) {
    nk_bool ret = nk_true;
    XEvent event;
    struct nk_context *ctx = &ui->ctx;

    NK_UNUSED(wait);
    if (ui->stop) return nk_false;

    /* if no context, then no font loaded, ensure all is loaded now */
    nkui__ensure_font_loaded(ui);

    ui->loop_started = nkui__timestamp();
    nk_input_begin(ctx);
    while (!ui->stop && ret && XPending(ui->dpy)) {
        XNextEvent(ui->dpy, &event);
        if (event.type == ClientMessage) {
            if (event.xclient.message_type == ui->atoms.wm_protocols &&
                event.xclient.data.l[0] == (long)ui->atoms.wm_delete_window) {
                ret = nk_false;
                break;
            }
        }
        if (XFilterEvent(&event, ui->xwin)) continue;

        /* mouse grabbing */
        if (ctx->input.mouse.grab) {
            XDefineCursor(ui->dpy, ui->xwin, ui->hidden_cursor);
            ctx->input.mouse.grab = 0;
        }
        else if (ctx->input.mouse.ungrab) {
            XWarpPointer(ui->dpy, None, ui->xwin, 0, 0, 0, 0,
                (int)ctx->input.mouse.prev.x, (int)ctx->input.mouse.prev.y);
            XUndefineCursor(ui->dpy, ui->xwin);
            ctx->input.mouse.ungrab = 0;
        }

        switch (event.type) {
        case KeyPress:
        case KeyRelease: {
            nk_bool down = (event.type == KeyPress);
            int ret;
            KeySym *code = XGetKeyboardMapping(ui->dpy, (KeyCode)event.xkey.keycode, 1, &ret);
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
                    long dt = nkui__timestamp() - ui->last_clicked;
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
            nk_input_motion(ctx, event.xmotion.x, event.xmotion.y);
            if (ctx->input.mouse.grabbed) {
                ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
                ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
                XWarpPointer(ui->dpy, None, ui->xwin, 0, 0, 0, 0,
                    (int)ctx->input.mouse.pos.x, (int)ctx->input.mouse.pos.y);
            }
        } break;
        case Expose:
        case ConfigureNotify: {
            XGetWindowAttributes(ui->dpy, ui->xwin, &ui->xwa);
            /* if (event.type == ConfigureNotify) { */
                XFreePixmap(ui->dpy, ui->dw);
                ui->dw = XCreatePixmap(ui->dpy, ui->xwin, ui->xwa.width, ui->xwa.height, ui->depth);
                XftDrawChange(ui->xft, ui->dw);
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
    nk_input_end(ctx);
    return ret;
}

NKUI_API void nkui_stop(struct nkui *ui) {
    ui->stop = nk_true;
}

NKUI_API void nkui_render(struct nkui *ui) {
    long dt;
    const struct nk_command *cmd;
    struct nk_context *ctx = &ui->ctx;

    XGetWindowAttributes(ui->dpy, ui->xwin, &ui->xwa);
    ui->draw(ui, ctx, ui->xwa.width, ui->xwa.height, ui->userdata);
    if (ui->stop) return;

    /* XClearWindow(nkui.dpy, nkui.xwin); */
    XSetForeground(ui->dpy, ui->gc, nkui__convert_color(ui->clear(ui->userdata)));
    XFillRectangle(ui->dpy, ui->dw, ui->gc, 0, 0, ui->xwa.width, ui->xwa.height);

    nk_foreach(cmd, ctx) {
        switch (cmd->type) {
        case NK_COMMAND_NOP:                break;
        case NK_COMMAND_SCISSOR:            nkui__draw_scissor(ui, (const struct nk_command_scissor *)cmd); break;
        case NK_COMMAND_LINE:               nkui__draw_line(ui, (const struct nk_command_line *)cmd); break;
        case NK_COMMAND_CURVE:              nkui__draw_curve(ui, (const struct nk_command_curve *)cmd); break;
        case NK_COMMAND_RECT:               nkui__draw_rect(ui, (const struct nk_command_rect *)cmd); break;
        case NK_COMMAND_RECT_FILLED:        nkui__draw_rect_filled(ui, (const struct nk_command_rect_filled *)cmd); break;
        case NK_COMMAND_RECT_MULTI_COLOR:   nkui__draw_rect_multi_color(ui, (const struct nk_command_rect_multi_color *)cmd); break;
        case NK_COMMAND_CIRCLE:             nkui__draw_circle(ui, (const struct nk_command_circle *)cmd); break;
        case NK_COMMAND_CIRCLE_FILLED:      nkui__draw_circle_filled(ui, (const struct nk_command_circle_filled *)cmd); break;
        case NK_COMMAND_ARC:                nkui__draw_arc(ui, (const struct nk_command_arc *)cmd); break;
        case NK_COMMAND_ARC_FILLED:         nkui__draw_arc_filled(ui, (const struct nk_command_arc_filled *)cmd); break;
        case NK_COMMAND_TRIANGLE:           nkui__draw_triangle(ui, (const struct nk_command_triangle *)cmd); break;
        case NK_COMMAND_TRIANGLE_FILLED:    nkui__draw_triangle_filled(ui, (const struct nk_command_triangle_filled *)cmd); break;
        case NK_COMMAND_POLYGON:            nkui__draw_polygon(ui, (const struct nk_command_polygon *)cmd); break;
        case NK_COMMAND_POLYGON_FILLED:     nkui__draw_polygon_filled(ui, (const struct nk_command_polygon_filled *)cmd); break;
        case NK_COMMAND_POLYLINE:           nkui__draw_polyline(ui, (const struct nk_command_polyline *)cmd); break;
        case NK_COMMAND_TEXT:               nkui__draw_text(ui, (const struct nk_command_text *)cmd); break;
        case NK_COMMAND_IMAGE:              nkui__draw_image(ui, (const struct nk_command_image *)cmd); break;
        case NK_COMMAND_CUSTOM:             nkui__draw_custom(ui, (const struct nk_command_custom *)cmd); break;
        }
    }
    nk_clear(ctx);

    XCopyArea(ui->dpy, ui->dw, ui->xwin, ui->gc, 0, 0, ui->xwa.width, ui->xwa.height, 0, 0);
    XFlush(ui->dpy);

    dt = nkui__timestamp() - ui->loop_started;
    if (dt < NKUI_MINLOOP_DELAY) nkui__sleep_for(NKUI_MINLOOP_DELAY - dt);
}

NKUI_API struct nk_font_atlas *nkui_font_begin(struct nkui *ui) {
    NK_UNUSED(ui);
    return NULL;
}

#ifdef NK_INCLUDE_DEFAULT_FONT
NKUI_API struct nk_font *nkui_font_load_default(struct nkui *ui, int size) {
    NK_UNUSED(ui);
    NK_UNUSED(size);
    return NULL;
}
#endif

#ifdef NK_INCLUDE_STANDARD_IO
NKUI_API struct nk_user_font *nkui_font_load_file(struct nkui *ui, const char *name, int size) {
    NK_UNUSED(ui);
    NK_UNUSED(name);
    NK_UNUSED(size);
    return NULL;
}
#endif

NKUI_API struct nk_user_font *nkui_font_load_native(struct nkui *ui, const char *name, int size) {
    char xftname[256];
    XftFont *xft;

    snprintf(xftname, 256, "%s-%d", name, size);
    xft = XftFontOpenName(ui->dpy, ui->screen, xftname);
    if (!xft) return NULL;
    return nkui__font_add(ui, xft);

}

NKUI_API void nkui_font_end(struct nkui *ui) {
    nkui__ensure_font_loaded(ui);
}

NKUI_API void nkui_font_free(struct nkui *ui, struct nk_user_font *font) {
    nkui__font_free(ui, font);
}

#ifdef NKUI_IMAGE_LOADER
NKUI_API struct nk_image nkui_image_load_file(struct nkui *ui, const char *filename) {
    int w, h, channels;
    unsigned char *data = stbi_load(filename, &w, &h, &channels, 0);
    return nkui__convert_stbi_image(ui, data, w, h, channels);
}

NKUI_API struct nk_image nkui_image_load_memory(struct nkui *ui, const unsigned char *membuf, nk_uint memsize) {
    int w, h, channels;
    unsigned char *data = stbi_load_from_memory(membuf, memsize, &w, &h, &channels, 0);
    return nkui__convert_stbi_image(ui, data, w, h, channels);
}

NKUI_API void nkui_image_free(struct nkui *ui, struct nk_image image) {
    struct nkui_image *uimage = image.handle.ptr;
    if (uimage) XDestroyImage(uimage->ximage);
    XFreePixmap(ui->dpy, uimage->mask);
    XFreeGC(ui->dpy, uimage->gc);
    free(uimage);
}
#endif /* NKUI_IMAGE_LOADER */

#ifdef __cplusplus
}
#endif

#endif /* NKUI_IMPLEMENTATION */
#endif /* NKUI_XLIB_H */