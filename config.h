#ifndef CONFIG_H
#define CONFIG_H

/* ---- Window ------------------------------------------------------------ */
#define TB_WIN_TITLE     "Toolbox"
#define TB_WIN_WIDTH     480
#define TB_WIN_HEIGHT    320

/* ---- Fonts (core X11 font names; first one that loads is used) --------- */
#define TB_FONT          "-misc-fixed-medium-r-normal--13-*-*-*-*-*-iso8859-1"
#define TB_FONT_FALLBACK "fixed"

/* ---- Layout (pixels) --------------------------------------------------- */
#define TB_MENUBAR_H     20   /* height of the menubar strip at the top  */
#define TB_MENU_ITEM_H   18   /* height of one dropdown item             */
#define TB_MENU_PAD_X    8    /* horizontal padding for titles and items */
#define TB_MENU_MIN_W    120  /* minimum dropdown width                  */
#define TB_BUTTON_BORDER 2    /* button outline thickness                */
#define TB_SCROLL_W      16   /* scrollbar thickness (and arrow box size) */
#define TB_SCROLL_MIN_THUMB 8 /* smallest thumb length                    */
#define TB_SCROLL_STEP   16   /* horizontal text scroll step, in pixels   */
#define TB_TEXT_PAD      2    /* inner padding of text widgets            */
#define TB_ROW_PAD       2    /* vertical padding inside list/tree rows   */
#define TB_CELL_PAD      4    /* horizontal padding inside list cells     */
#define TB_TREE_INDENT   14   /* indent per tree level                    */
#define TB_TREE_DISC     12   /* size of the expand/collapse triangle     */
#define TB_DBLCLICK_MS   400  /* double-click interval                    */

/* ---- 16-colour palette (terminal / ANSI order) ------------------------- */
enum {
    TB_BLACK,    /*  0 */
    TB_MAROON,   /*  1 */
    TB_GREEN,    /*  2 */
    TB_OLIVE,    /*  3 */
    TB_NAVY,     /*  4 */
    TB_PURPLE,   /*  5 */
    TB_TEAL,     /*  6 */
    TB_SILVER,   /*  7 */
    TB_GRAY,     /*  8 */
    TB_RED,      /*  9 */
    TB_LIME,     /* 10 */
    TB_YELLOW,   /* 11 */
    TB_BLUE,     /* 12 */
    TB_FUCHSIA,  /* 13 */
    TB_AQUA,     /* 14 */
    TB_WHITE     /* 15 */
};

static const char *const tb_palette[16] = {
    "#000000", "#800000", "#008000", "#808000",
    "#000080", "#800080", "#008080", "#c0c0c0",
    "#808080", "#ff0000", "#00ff00", "#ffff00",
    "#0000ff", "#ff00ff", "#00ffff", "#ffffff"
};

/* ---- Semantic roles (map the palette onto the toolbox look) ------------- */
#define TB_C_BG          TB_WHITE   /* window and menubar background */
#define TB_C_FG          TB_BLACK   /* text, borders, lines          */
#define TB_C_SEL_BG      TB_BLACK   /* selected menu title / item    */
#define TB_C_SEL_FG      TB_WHITE

#endif /* CONFIG_H */
