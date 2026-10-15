#ifndef TB_SCROLLWIN_H
#define TB_SCROLLWIN_H

/*
 * tb_scrollwin.h - scroll window: a viewport onto a larger area of child
 * widgets. Children are positioned in content coordinates (0,0 is the
 * top-left of the content). Scroll bars appear only when the content is
 * larger than the window.
 *
 *   tb_scrollwin *sw = tb_scrollwin_new(t, x, y, w, h, contentW, contentH);
 *   tb_container *prev = tb_build_begin(t, &sw->c);
 *   tb_checkbox_new(t, 10, 10, "A", NULL, NULL);     // content coordinates
 *   tb_build_end(t, prev);
 *
 * Children receive pointer and key events while the scroll window is
 * focused. Keyboard Tab cycles top-level widgets, so reach a child by
 * clicking it.
 */

#include "tb_core.h"
#include "tb_scroll.h"

typedef struct tb_scrollwin {
    tb_widget base;
    tb_container c;          /* children, in content coordinates        */
    int cw, ch;              /* content size                            */
    tb_scroll sbv, sbh;
    int vis_v, vis_h;        /* which bars are shown                    */
    int vw, vh;              /* viewport size (window minus bars)       */
    tb_widget *cgrab;        /* child receiving drags, or NULL          */
    int bar_grab;            /* 0 none, 1 vertical bar, 2 horizontal    */
} tb_scrollwin;

/* Work out bar visibility, viewport size and bar geometry. */
static inline void tb_sw_sync(tb_scrollwin *sw) {
    int B = TB_SCROLL_W;
    int w = sw->base.w, h = sw->base.h;
    int vv = sw->ch > h, vh = sw->cw > w;
    if (vh) vv = sw->ch > h - B;
    if (vv) vh = sw->cw > w - B;
    sw->vis_v = vv;
    sw->vis_h = vh;
    sw->vw = w - (vv ? B : 0);
    sw->vh = h - (vh ? B : 0);
    if (sw->vw < 1) sw->vw = 1;
    if (sw->vh < 1) sw->vh = 1;

    sw->sbv.x = sw->base.x + w - B;
    sw->sbv.y = sw->base.y;
    sw->sbv.w = B;
    sw->sbv.h = sw->vh;
    sw->sbv.vertical = 1;
    tb_sb_set(&sw->sbv, sw->ch, sw->vh);
    sw->sbv.step = 1;

    sw->sbh.x = sw->base.x;
    sw->sbh.y = sw->base.y + h - B;
    sw->sbh.w = sw->vw;
    sw->sbh.h = B;
    sw->sbh.vertical = 0;
    tb_sb_set(&sw->sbh, sw->cw, sw->vw);
    sw->sbh.step = TB_SCROLL_STEP;
}

static inline void tb_sw_draw(tb_app *t, tb_widget *wb, int focused) {
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    (void)focused;
    tb_sw_sync(sw);

    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);

    /* children, drawn shifted by the scroll position and clipped to the
     * viewport */
    int ox = t->ox, oy = t->oy;
    XRectangle saved = t->region;
    XRectangle vp = tb_rect(wb->x + ox, wb->y + oy, sw->vw, sw->vh);
    tb_region_set(t, tb_isect(saved, vp));
    t->ox = ox + wb->x - sw->sbh.value;
    t->oy = oy + wb->y - sw->sbv.value;
    tb_draw_kids(t, &sw->c);
    t->ox = ox;
    t->oy = oy;
    tb_region_set(t, saved);

    if (sw->vis_v) tb_sb_draw(t, &sw->sbv);
    if (sw->vis_h) tb_sb_draw(t, &sw->sbh);
    if (sw->vis_v && sw->vis_h)
        tb_fill(t, wb->x + wb->w - TB_SCROLL_W, wb->y + wb->h - TB_SCROLL_W,
                TB_SCROLL_W, TB_SCROLL_W, TB_C_BG);
    tb_frame(t, wb->x, wb->y, wb->w, wb->h, TB_C_FG);
}

/* Pointer position in content coordinates. */
static inline int tb_sw_cx(const tb_scrollwin *sw, int x) { return x - sw->base.x + sw->sbh.value; }
static inline int tb_sw_cy(const tb_scrollwin *sw, int y) { return y - sw->base.y + sw->sbv.value; }

static inline int tb_sw_press(tb_app *t, tb_widget *wb, int x, int y) {
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    tb_sw_sync(sw);
    if (sw->vis_v && tb_sb_hit(&sw->sbv, x, y)) {
        sw->bar_grab = 1;
        tb_sb_press(&sw->sbv, x, y);
        return 1;
    }
    if (sw->vis_h && tb_sb_hit(&sw->sbh, x, y)) {
        sw->bar_grab = 2;
        tb_sb_press(&sw->sbh, x, y);
        return 1;
    }
    if (x >= wb->x + sw->vw || y >= wb->y + sw->vh) return 0;   /* in the corner */

    int cx = tb_sw_cx(sw, x), cy = tb_sw_cy(sw, y);
    tb_widget *k = tb_hit_in(&sw->c, cx, cy);
    if (!k) return 0;
    if (k->focusable) sw->c.focus = k;
    if (k->ops->press && k->ops->press(t, k, cx, cy)) {
        sw->cgrab = k;
        return 1;
    }
    return 0;
}

static inline void tb_sw_motion(tb_app *t, tb_widget *wb, int x, int y) {
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    if (sw->bar_grab == 1) { tb_sb_drag(&sw->sbv, x, y); return; }
    if (sw->bar_grab == 2) { tb_sb_drag(&sw->sbh, x, y); return; }
    if (sw->cgrab && sw->cgrab->ops->motion)
        sw->cgrab->ops->motion(t, sw->cgrab, tb_sw_cx(sw, x), tb_sw_cy(sw, y));
}

static inline void tb_sw_release(tb_app *t, tb_widget *wb, int x, int y) {
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    if (sw->bar_grab == 1) sw->sbv.dragging = 0;
    if (sw->bar_grab == 2) sw->sbh.dragging = 0;
    sw->bar_grab = 0;
    if (sw->cgrab) {
        tb_widget *k = sw->cgrab;
        sw->cgrab = NULL;
        if (k->ops->release) k->ops->release(t, k, tb_sw_cx(sw, x), tb_sw_cy(sw, y));
    }
}

static inline void tb_sw_wheel(tb_app *t, tb_widget *wb, int dir) {
    (void)t;
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    tb_sw_sync(sw);
    tb_sb_adjust(&sw->sbv, dir * 3);
}

static inline void tb_sw_key(tb_app *t, tb_widget *wb, XKeyEvent *e) {
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    tb_widget *f = sw->c.focus;
    if (f && f->ops->key) f->ops->key(t, f, e);
}

static inline void tb_sw_destroy(tb_widget *wb) {
    tb_scrollwin *sw = (tb_scrollwin *)wb;
    for (int i = 0; i < sw->c.n; i++)
        sw->c.kids[i]->ops->destroy(sw->c.kids[i]);
    free(sw->c.kids);
    free(sw);
}

static const tb_wops TB_SCROLLWIN_OPS = {
    tb_sw_draw, tb_sw_press, tb_sw_motion, tb_sw_release,
    tb_sw_wheel, tb_sw_key, tb_sw_destroy, NULL
};

static inline tb_scrollwin *tb_scrollwin_new(tb_app *t, int x, int y, int w, int h,
                                             int cw, int ch) {
    tb_scrollwin *sw = calloc(1, sizeof *sw);
    if (!sw) tb_oom();
    sw->base.ops = &TB_SCROLLWIN_OPS;
    sw->base.x = x;
    sw->base.y = y;
    sw->base.w = w;
    sw->base.h = h;
    sw->base.clickable = 1;
    sw->base.focusable = 1;
    sw->cw = cw;
    sw->ch = ch;
    sw->bar_grab = 0;
    sw->sbv.step = 1;
    sw->sbh.step = TB_SCROLL_STEP;
    tb_add_widget(t, &sw->base);
    return sw;
}

/* Change the content size (e.g. after adding children). */
static inline void tb_scrollwin_resize(tb_scrollwin *sw, int cw, int ch) {
    sw->cw = cw;
    sw->ch = ch;
    if (sw->base.app) tb_damage_all(sw->base.app);
}

#endif /* TB_SCROLLWIN_H */
