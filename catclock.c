#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "config.h"
#include "toolbox.h"

/* Catclock: an analogue clock with a sitting cat on its face. Loosely after
 * Plan 9's catclock. The second hand is a plain red hand, not the cat's tail.
 * Redraws four times a second from a timer. */

#define CC_SIZE 200
#define CAT_W 24
#define CAT_H 18
#define TICK_MS 250

/* 24 x 18 cat face, 1 bit per pixel, rows of 3 bytes, MSB first. */
static const unsigned char cat_bits[CAT_H * 3] = {
    0x00, 0x00, 0x00,
    0x08, 0x00, 0x10,
    0x0c, 0x00, 0x30,
    0x0e, 0x00, 0x70,
    0x1f, 0xff, 0xf8,
    0x1f, 0xff, 0xf8,
    0x1f, 0xff, 0xf8,
    0x3f, 0xff, 0xfc,
    0x3f, 0xff, 0xfc,
    0x7f, 0x7e, 0xfe,
    0x3f, 0x7e, 0xfc,
    0x3f, 0xff, 0xfc,
    0x3f, 0xe7, 0xfc,
    0x3f, 0xff, 0xfc,
    0x1f, 0xff, 0xf8,
    0x0f, 0xff, 0xf0,
    0x07, 0xff, 0xe0,
    0x01, 0xff, 0x80
};

static tb_widget *face;
static tb_icon *cat;

/* A hand from the centre at angle a (radians, 0 = twelve o'clock). */
static void hand(tb_app *t, int cx, int cy, double a, double len, int thick, int color) {
    int ex = cx + (int)lround(len * sin(a));
    int ey = cy - (int)lround(len * cos(a));
    tb_draw_line(t, cx, cy, ex, ey, color);
    if (thick)   /* a second line beside it makes the hour hand heavier */
        tb_draw_line(t, cx + 1, cy, ex + 1, ey, color);
}

static void face_draw(tb_app *t, tb_widget *wb, int focused) {
    (void)focused;
    int cx = wb->x + wb->w / 2, cy = wb->y + wb->h / 2;
    int R = (wb->w < wb->h ? wb->w : wb->h) / 2 - 2;

    tb_fill(t, wb->x, wb->y, wb->w, wb->h, TB_C_BG);
    tb_arc(t, cx - R, cy - R, 2 * R, 2 * R, TB_C_FG, 0);

    /* twelve hour ticks */
    for (int i = 0; i < 12; i++) {
        double a = i * M_PI / 6.0;
        int outer = R - 2, inner = R - (i % 3 == 0 ? 10 : 6);
        tb_draw_line(t, cx + (int)lround(inner * sin(a)), cy - (int)lround(inner * cos(a)),
                     cx + (int)lround(outer * sin(a)), cy - (int)lround(outer * cos(a)),
                     TB_C_FG);
    }

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    int h = tmv.tm_hour % 12, m = tmv.tm_min, s = tmv.tm_sec;

    hand(t, cx, cy, (h + m / 60.0) * M_PI / 6.0, R * 0.5, 1, TB_C_FG);   /* hour   */
    hand(t, cx, cy, (m + s / 60.0) * M_PI / 30.0, R * 0.8, 0, TB_C_FG);  /* minute */
    hand(t, cx, cy, s * M_PI / 30.0, R * 0.9, 0, TB_RED);                /* second */
}

static void face_destroy(tb_widget *wb) {
    free(wb);
}

static const tb_wops FACE_OPS = {
    face_draw, NULL, NULL, NULL, NULL, NULL, face_destroy, NULL
};

static tb_widget *face_new(tb_app *t, int x, int y, int w, int h) {
    tb_widget *wb = calloc(1, sizeof *wb);
    if (!wb) tb_oom();
    wb->ops = &FACE_OPS;
    wb->x = x;
    wb->y = y;
    wb->w = w;
    wb->h = h;
    tb_add_widget(t, wb);
    return wb;
}

/* Face fills the window; the cat sits below the centre, like the hands' pivot. */
static void layout(tb_app *app) {
    face->w = app->width;
    face->h = app->height;
    cat->base.x = (app->width - CAT_W) / 2;
    cat->base.y = app->height / 2 + 28;
    tb_damage_all(app);
}

static void on_resize(tb_app *app, void *data) {
    (void)data;
    layout(app);
}

static void tick(tb_app *app, void *data) {
    tb_damage(app, (tb_widget *)data);
}

int main(void) {
    tb_app *app = tb_app_new("Catclock", CC_SIZE, CC_SIZE);
    if (!app) return 1;

    face = face_new(app, 0, 0, app->width, app->height);
    cat = tb_icon_new(app, 0, 0, CAT_W, CAT_H, cat_bits, TB_BLACK);
    layout(app);
    tb_on_resize(app, on_resize, NULL);

    tb_timer *timer = tb_timer_new(app, TICK_MS, tick, face);
    if (!timer) {
        fprintf(stderr, "catclock: cannot create timer\n");
        tb_app_free(app);
        return 1;
    }

    tb_run(app);
    tb_timer_free(timer);
    tb_app_free(app);
    return 0;
}
