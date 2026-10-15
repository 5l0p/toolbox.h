#ifndef TB_CLIP_H
#define TB_CLIP_H

/*
 * tb_clip.h - X clipboard (the CLIPBOARD selection).
 *
 *   tb_clip_copy()   stores text and claims ownership of CLIPBOARD.
 *   tb_clip_paste()  asks for CLIPBOARD and delivers it to the focused
 *                    widget's text_in() hook. If we own the selection the
 *                    text is delivered immediately; otherwise it arrives
 *                    later as a SelectionNotify event handled by the loop.
 *
 * Other clients get our text through SelectionRequest events, which the
 * event loop passes to tb_clip_on_request(). Targets offered: UTF8_STRING,
 * STRING and TARGETS. Text is ASCII-filtered by the receiving widget.
 */

#include "tb_core.h"

static inline void tb_clip_init(tb_app *t) {
    t->a_clip = XInternAtom(t->dpy, "CLIPBOARD", False);
    t->a_utf8 = XInternAtom(t->dpy, "UTF8_STRING", False);
    t->a_targets = XInternAtom(t->dpy, "TARGETS", False);
    t->a_string = XInternAtom(t->dpy, "STRING", False);
    t->a_prop = XInternAtom(t->dpy, "TB_PASTE", False);
}

/* Copy n bytes of text to the clipboard and take CLIPBOARD ownership. */
static inline void tb_clip_copy(tb_app *t, const char *s, int n) {
    char *p = malloc((size_t)n + 1);
    if (!p) tb_oom();
    memcpy(p, s, (size_t)n);
    p[n] = 0;
    free(t->clip);
    t->clip = p;
    t->cliplen = n;
    XSetSelectionOwner(t->dpy, t->a_clip, t->win, CurrentTime);
}

/* Hand text to whichever widget asked for a paste. */
static inline void tb_clip_deliver(tb_app *t, const char *s, int n) {
    tb_widget *w = t->paste_target;
    t->paste_target = NULL;
    if (w && w->text_in && n > 0) w->text_in(t, w, s, n);
}

/* Start a paste into the active container's focused widget. */
static inline void tb_clip_paste(tb_app *t) {
    t->paste_target = tb_active(t)->focus;
    if (!t->paste_target) return;
    if (XGetSelectionOwner(t->dpy, t->a_clip) == t->win) {
        /* We own the selection: no need to round-trip through the server. */
        tb_clip_deliver(t, t->clip ? t->clip : "", t->cliplen);
        return;
    }
    XConvertSelection(t->dpy, t->a_clip, t->a_utf8, t->a_prop, t->win, CurrentTime);
}

/* SelectionRequest: another client wants our clipboard contents. */
static inline void tb_clip_on_request(tb_app *t, XSelectionRequestEvent *rq) {
    XSelectionEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SelectionNotify;
    ev.display = rq->display;
    ev.requestor = rq->requestor;
    ev.selection = rq->selection;
    ev.target = rq->target;
    ev.time = rq->time;
    ev.property = None;

    Atom prop = rq->property != None ? rq->property : rq->target;
    if (rq->selection == t->a_clip) {
        if (rq->target == t->a_targets) {
            Atom list[3] = { t->a_targets, t->a_utf8, t->a_string };
            XChangeProperty(t->dpy, rq->requestor, prop, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)list, 3);
            ev.property = prop;
        } else if (rq->target == t->a_utf8 || rq->target == t->a_string) {
            XChangeProperty(t->dpy, rq->requestor, prop, rq->target, 8,
                            PropModeReplace,
                            (unsigned char *)(t->clip ? t->clip : ""), t->cliplen);
            ev.property = prop;
        }
    }
    XSendEvent(t->dpy, rq->requestor, False, NoEventMask, (XEvent *)&ev);
}

/* SelectionNotify: the answer to our XConvertSelection request. */
static inline void tb_clip_on_notify(tb_app *t, XSelectionEvent *ev) {
    if (ev->selection != t->a_clip) return;
    if (ev->property == None) {          /* owner refused or no such target */
        t->paste_target = NULL;
        return;
    }
    Atom type;
    int format = 0;
    unsigned long nitems = 0, after = 0;
    unsigned char *data = NULL;
    if (XGetWindowProperty(t->dpy, t->win, t->a_prop, 0, 1L << 20, True,
                           AnyPropertyType, &type, &format, &nitems, &after,
                           &data) == Success && data && format == 8) {
        tb_clip_deliver(t, (const char *)data, (int)nitems);
    } else {
        t->paste_target = NULL;
    }
    if (data) XFree(data);
}

#endif /* TB_CLIP_H */
