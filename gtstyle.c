/* gtstyle.c: Style formatting hints.
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_grid.h"
#include "gtw_buf.h"
/* This has a lot of macros that conflict with existing code, so we include it
    later. */
#include <term.h>

static struct rgb_name_struct {
    const char *name;
    glsi32 rgb;
} rgb_names[] = {
#include "cssrgb.h"
};

#if defined(NCURSES_VERSION) && (NCURSES_VERSION_PATCH >= 20170401)
#define INIT_PAIR(pair, f, b) init_extended_pair((pair), (f), (b))
#define ATTR_SET(attrs, pair) attr_set((attrs), (pair), &(pair))
#define PAIR_MAX INT_MAX
#else
#define INIT_PAIR(pair, f, b) init_pair((pair), (f), (b))
#define ATTR_SET(attrs, pair) attr_set((attrs), (pair), NULL)
#define PAIR_MAX SHRT_MAX
#endif /* defined(NCURSES_VERSION) && (NCURSES_VERSION_PATCH >= 20170401) */

#if defined(NCURSES_VERSION) && (NCURSES_VERSION_PATCH >= 20200411)
/* Earlier versions of ncurses had a broken find_pair and alloc_pair. */
#define USE_NCURSES_ALLOC_PAIR
#endif /* defined(NCURSES_VERSION) && (NCURSES_VERSION_PATCH >= 20200411) */

/* If we don't have italics, use underline. (This is also a command-line option
    when italics are available.) */
#ifndef A_ITALIC
#define A_ITALIC A_UNDERLINE
#endif

#define COUNTOF(ar) (sizeof(ar) / sizeof((ar)[0]))
#define COMPILE_TIME_ASSERT2(b, line) \
    typedef int compile_time_assert_failed_at_line_##line[(b) ? 1 : -1]
#define COMPILE_TIME_ASSERT1(b, line) COMPILE_TIME_ASSERT2(b, line)
#define COMPILE_TIME_ASSERT(b) COMPILE_TIME_ASSERT1(b, __LINE__)

/* Get the red, green, and blue parts of a color. */
#define R_PART(color) (((glsi32)color >> 16) & 0xFF)
#define G_PART(color) (((glsi32)color >> 8) & 0xFF)
#define B_PART(color) ((glsi32)color & 0xFF)
#define SCALE_255_1000(byte) (((int)(byte) * 1000 + 127) / 255)

/* We can't really know the actual colors the user sees. These are taken from
    XTerm. */
static const glsi32 xterm_16colors[] = {
    0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD,
    0x00CDCD, 0xE5E5E5, 0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00,
    0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
};
COMPILE_TIME_ASSERT(COUNTOF(xterm_16colors) == 16);

/* The steps in the xterm-256color 6x6 color cube. */
static const uint8_t xterm_256color_steps[] = {
    0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF
};
COMPILE_TIME_ASSERT(COUNTOF(xterm_256color_steps) == 6);

typedef struct stylehint_struct {
    glsi32 weight;
    glsi32 oblique;
    glsi32 textcolor;
    glsi32 backcolor;
    glsi32 reversecolor;
    int textcolori;
    int backcolori;
} stylehint_t;
static stylehint_t textbuffer_stylehints[style_NUMSTYLES];
static stylehint_t textgrid_stylehints[style_NUMSTYLES];

#ifndef USE_NCURSES_ALLOC_PAIR
/* A list with all the pairs. In lieu of a proper map, we look for the correct
    color indexes to get the pair number. */
static struct pair_struct {
    int pair; /* Pair number */
    int fgi; /* Index for foreground (depends on terminal) */
    int bgi; /* Index for foreground (depends on terminal) */
    struct pair_struct *next;
} *pairs_head = NULL;
#endif /* USE_NCURSES_ALLOC_PAIR */

static int alloc_curses_pair(int fgi, int bgi)
{
#ifdef USE_NCURSES_ALLOC_PAIR
    int ret = alloc_pair(fgi, bgi);
    return (ret >= 0) ? ret : 0;
#else
    struct pair_struct *node = NULL, *prev_node = NULL;
    int pair = 1, count = 0;
    if (fgi == -1 && bgi == -1) {
        return 0;
    }
    for ((void)(node = pairs_head), prev_node = NULL; node;
         prev_node = node, node = node->next) {
        ++count;
        if (node->fgi == fgi && node->bgi == bgi) {
            if (prev_node) {
                prev_node->next = node->next;
                node->next = pairs_head;
                pairs_head = node;
            }
            return node->pair;
        }
        if (node->pair >= pair) {
            pair = node->pair + 1;
        }
        /* When we're out of pairs, replace the least-recently used one. */
        if (count >= PAIR_MAX) {
            pair = node->pair;
            prev_node->next = node->next;
            free(node);
            break;
        }
    }
    if (INIT_PAIR(pair, fgi, bgi) == ERR) {
        return 0;
    }
    node = malloc(sizeof(struct pair_struct));
    node->pair = pair;
    node->fgi = fgi;
    node->bgi = bgi;
    node->next = pairs_head;
    pairs_head = node;
    return pair;
#endif /* USE_NCURSES_ALLOC_PAIR */
}

static int color_distance_squared(int r1, int g1, int b1,
                                  int r2, int g2, int b2)
{
    return ((r2 - r1) * (r2 - r1) +
            (g2 - g1) * (g2 - g1) +
            (b2 - b1) * (b2 - b1));
}

static void rgblevel_to_6x6(int level, int *index, uint8_t *out)
{
    if (level < 48) {
        *index = 0;
    } else if (level < 115) {
        *index = 1;
    } else {
        *index = (level - 35) / 40;
    }
    *out = xterm_256color_steps[*index];
}

static int get_nearest_curses_color(glsi32 color)
{
    typedef struct {
        glsi32 color;
        int curses_color;
    } curses_color_cache_t;
    /* Cache a few recent colors. */
    static curses_color_cache_t cache[] = {{-1}, {-1}, {-1}, {-1}};
    static curses_color_cache_t *cache_write = cache;
    int i, r, g, b, ret = -1;

    if (COLORS < 8 || color < 0 || color > 0xFFFFFF) {
        return -1; /* Tell curses to use the default. */
    }

    for (i = 0; i < (int)(COUNTOF(cache)); ++i) {
        if (cache[i].color == color) {
            return cache[i].curses_color;
        }
    }

    r = R_PART(color);
    g = G_PART(color);
    b = B_PART(color);

    if (COLORS >= 0x1000000) {
        /* The xterm-direct terminal description keeps 0 through 7 as indexed 
            colors. */
        if (color <= 3) {
            color = 0;
        } else if (color <= 7) {
            color = 8;
        }
        ret = color;
    } else if (COLORS == 256) {
        /* Assume it uses the standard 6x6 color cube and grayscale ramp. */
        int ri, gi, bi;
        uint8_t ro, go, bo;
        rgblevel_to_6x6(r, &ri, &ro);
        rgblevel_to_6x6(g, &gi, &go);
        rgblevel_to_6x6(b, &bi, &bo);
        if (ro != r || go != g || bo != b) {
            int grayo;
            int grayi = ((r + g + b) / 3 - 3) / 10;
            if (grayi > 23) {
                grayi = 23;
            }
            grayo = (10 * grayi) + 8;
            if (color_distance_squared(r, g, b, grayo, grayo, grayo) <=
                color_distance_squared(r, g, b, ro, go, bo)) {
                ret = grayi + 232;
            }
        }
        if (ret < 0) {
            ret = (36 * ri) + (6 * gi) + bi + 16;
        }
    } else if (COLORS >= 8) {
        int best_i = -1, distance_squared = 0, best_distance_squared = 0;
        int n = COUNTOF(xterm_16colors);
        if (n > COLORS) {
            n = COLORS;
        }
        for (i = 0; i < n; ++i) {
            distance_squared = color_distance_squared(r, g, b,
                R_PART(xterm_16colors[i]),
                G_PART(xterm_16colors[i]),
                B_PART(xterm_16colors[i]));
            if (best_i < 0 || distance_squared < best_distance_squared) {
                best_i = i;
                best_distance_squared = distance_squared;
            }
            if (distance_squared == 0) {
                break;
            }
        }
        ret = best_i;
    } else {
        return -1; /* Tell curses to use the default. */
    }

    cache_write->color = color;
    cache_write->curses_color = ret;
    if (++cache_write >= cache + COUNTOF(cache)) {
        cache_write = cache;
    } 
    return ret;
}

void gli_initialize_styles(void)
{
    int i, colori;

    if (pref_color && start_color() != ERR) {
        use_default_colors();
        if (can_change_color()) {
            /* Reset the terminal's colors. We'll also set the colors to known
                defaults. */
            putp(orig_colors);
            fflush(stdout);
            /* Set the first 8 or 16 colors to match what we expect. */
            if (COLORS >= 8) {
                int i, n = COUNTOF(xterm_16colors);
                if (n > COLORS) {
                    n = COLORS;
                }
                if (COLORS == 0x1000000) {
                    /* Only reset the first 8 colors for direct color. */
                    n = 8;
                }
                for (i = 0; i < n; ++i) {
                    init_color(i,
                               SCALE_255_1000(R_PART(xterm_16colors[i])),
                               SCALE_255_1000(G_PART(xterm_16colors[i])),
                               SCALE_255_1000(B_PART(xterm_16colors[i])));
                }
                if (COLORS == 256) {
                    int ri, gi, bi, n = COUNTOF(xterm_256color_steps);
                    /* Reset the 6x6 color cube to what we expect. */
                    for ((void)(ri = 0), i = 16; ri < n; ++ri) {
                        for (gi = 0; gi < n; ++gi) {
                            for (bi = 0; bi < n; ++bi) {
                                init_color(
                                    i++,
                                    SCALE_255_1000(xterm_256color_steps[ri]),
                                    SCALE_255_1000(xterm_256color_steps[gi]),
                                    SCALE_255_1000(xterm_256color_steps[bi]));
                            }
                        }
                    }
                    /* Reset the grayscale ramp to what we expect. */
                    for (i = 232; i < 256; ++i) {
                        gi = SCALE_255_1000(8 + 10 * (i - 232));
                        init_color(i, gi, gi, gi);
                    }
                }
            }
        }
    }

    for (i = 0; i < style_NUMSTYLES; ++i) {
        stylehint_t *textbuffer_stylehint = &textbuffer_stylehints[i];
        stylehint_t *textgrid_stylehint = &textgrid_stylehints[i];

        /* Inform 7 uses style_Header for [bold type]. */
        if (i == style_Header || i == style_Subheader ||
            i == style_Alert || i == style_Input) {
            textbuffer_stylehint->weight = 1;
            textgrid_stylehint->weight = 1;
        }

        /* Inform 7 uses style_Emphasized for [italic type]. */
        if (i == style_Emphasized || i == style_Alert || i == style_Note) {
            textbuffer_stylehint->oblique = 1;
            textgrid_stylehint->oblique = 1;
        }

        textbuffer_stylehint->textcolor = pref_fgcolor;
        textgrid_stylehint->textcolor = pref_fgcolor;

        textbuffer_stylehint->backcolor = pref_bgcolor;
        textgrid_stylehint->backcolor = pref_bgcolor;

        textbuffer_stylehint->reversecolor = 0;
        textgrid_stylehint->reversecolor = pref_reverse_textgrids ? 1 : 0;

        colori = get_nearest_curses_color(pref_fgcolor);
        textbuffer_stylehint->textcolori = colori;
        textgrid_stylehint->textcolori = colori;

        colori = get_nearest_curses_color(pref_bgcolor);
        textbuffer_stylehint->backcolori = colori;
        textgrid_stylehint->backcolori = colori;
    }
}

void gli_initialize_window_styles(window_t *win)
{
    stylehint_t *stylehints = NULL;
    int i = 0;
    if (win->type == wintype_TextBuffer) {
        stylehints = textbuffer_stylehints;
    } else if (win->type == wintype_TextGrid) {
        stylehints = textgrid_stylehints;
    } else {
        return;
    }
    win->stylehints = malloc(sizeof(stylehint_t) * style_NUMSTYLES);
    for (i = 0; i < style_NUMSTYLES; ++i) {
        win->stylehints[i] = stylehints[i];
    }
    gli_reset_styleplus(&win->styleplus, style_Normal);
}

int gli_compare_styles(const styleplus_t *styleplus1,
                       const styleplus_t *styleplus2)
{
    if (styleplus1->style < styleplus2->style) {
        return -1;
    }
    if (styleplus1->style > styleplus2->style) {
        return 1;
    }
    if (styleplus1->inline_fgcolor < styleplus2->inline_fgcolor) {
        return -1;
    }
    if (styleplus1->inline_fgcolor > styleplus2->inline_fgcolor) {
        return 1;
    }
    if (styleplus1->inline_bgcolor < styleplus2->inline_bgcolor) {
        return -1;
    }
    if (styleplus1->inline_bgcolor > styleplus2->inline_bgcolor) {
        return 1;
    }
    if (styleplus1->inline_reverse < styleplus2->inline_reverse) {
        return -1;
    }
    if (styleplus1->inline_reverse > styleplus2->inline_reverse) {
        return 1;
    }
    return 0;
}

static int compare_rgb_names(const void *lhs, const void *rhs)
{
    return strcmp((const char *)lhs, ((struct rgb_name_struct *)rhs)->name);
}

/* Sets color to [0, 0xFFFFFF] (color) or -1 if successful.
    Returns TRUE if successful, FALSE otherwise. */
int gli_get_color_for_name(const char *name, glsi32 *color)
{
    size_t len = strlen(name);
    unsigned int r, g, b;
    size_t i;
    char *name_lower;
    struct rgb_name_struct *rgb_name;

    if (len == 7 && sscanf(name, "#%2X%2X%2X", &r, &g, &b) == 3) {
        *color = (r << 16) | (g << 8) | b;
        return TRUE;
    } else if (len == 4 && sscanf(name, "#%1X%1X%1X", &r, &g, &b) == 3) {
        *color = ((r * 0x11) << 16) | ((g * 0x11) << 8) | (b * 0x11);
        return TRUE;
    }

    name_lower = strdup(name);
    for (i = 0; name_lower[i]; ++i) {
        name_lower[i] = tolower(name_lower[i]);
    }
    rgb_name = bsearch(name_lower, rgb_names, COUNTOF(rgb_names),
                       sizeof(rgb_names[0]), compare_rgb_names);
    free(name_lower);
    if (!rgb_name) {
        return FALSE;
    }
    *color = rgb_name->rgb;
    return TRUE;
}

void gli_reset_styleplus(styleplus_t *styleplus, glui32 style)
{
    styleplus->style = style;
    styleplus->inline_fgcolor = -1;
    styleplus->inline_bgcolor = -1;
    styleplus->inline_reverse = 0;
    styleplus->inline_fgi = -1;
    styleplus->inline_bgi = -1;
}

/* If styleplus is NULL, use Normal. */
int gli_set_window_style(window_t *win, const styleplus_t *styleplus)
{
    const stylehint_t *stylehint;
    attr_t attr;
    int fgi, bgi, pair;
    stylehint = styleplus ?
        &win->stylehints[styleplus->style] : &win->stylehints[style_Normal];

    attr = 0;
    if (stylehint->weight > 0) {
        attr |= A_BOLD;
    } else if (stylehint->weight < 0) {
        attr |= A_DIM;
    }
    if (stylehint->oblique) {
        attr |= (pref_emph_underline ? A_UNDERLINE : A_ITALIC);
    }
    fgi = stylehint->textcolori;
    bgi = stylehint->backcolori;
    if (stylehint->reversecolor ||
        (win->type == wintype_TextGrid && pref_reverse_textgrids) ||
        (styleplus && styleplus->inline_reverse)) {
        attr |= A_REVERSE;
    }

    if (styleplus) {
        if (styleplus->inline_fgcolor >= 0) {
            fgi = styleplus->inline_fgi;
        }
        if (styleplus->inline_bgcolor >= 0) {
            bgi = styleplus->inline_bgi;
        }
        if (styleplus->inline_reverse) {
           attr |= A_REVERSE;
        }
    }

    pair = alloc_curses_pair(fgi, bgi);
    return (ATTR_SET(attr, pair) == OK);
}

#ifdef GLK_MODULE_GARGLKTEXT

void gli_set_inline_colors(stream_t *str, glui32 fg, glui32 bg)
{
    if (!pref_stylehint ||
        !str || !str->writable || str->type != strtype_Window)
        return;

    if ((glsi32)fg >= -1 && (glsi32)fg <= 0xFFFFFF) {
        str->win->styleplus.inline_fgcolor = fg;
        str->win->styleplus.inline_fgi = get_nearest_curses_color(fg);
    }
    if ((glsi32)bg >= -1 && (glsi32)bg <= 0xFFFFFF) {
        str->win->styleplus.inline_bgcolor = bg;
        str->win->styleplus.inline_bgi = get_nearest_curses_color(bg);
    }

    if (str->win->echostr) {
        gli_set_inline_colors(str->win->echostr, fg, bg);
    }
}

void gli_set_inline_reverse(stream_t *str, glui32 reverse)
{
    if (!pref_stylehint ||
        !str || !str->writable || str->type != strtype_Window) {
        return;
    }

    str->win->styleplus.inline_reverse = reverse;
    if (str->win->echostr)
        gli_set_inline_reverse(str->win->echostr, reverse);
}

#endif /* GLK_MODULE_GARGLKTEXT */

void gli_destroy_window_styles(window_t *win)
{
    free(win->stylehints);
}

void gli_shutdown_styles(void)
{
#ifndef USE_NCURSES_ALLOC_PAIR
    struct pair_struct *pair_node = pairs_head, *tmp = NULL;
    for (; pair_node; pair_node = tmp) {
        tmp = pair_node->next;
        free(pair_node);
    }
#endif /* USE_NCURSES_ALLOC_PAIR */

    if (can_change_color()) {
        putp(orig_colors);
        fflush(stdout);
    }
}

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
    stylehint_t *stylehint = NULL;

    if (!pref_stylehint ||
        styl >= style_NUMSTYLES || hint >= stylehint_NUMHINTS) {
        return;
    }

    switch (wintype) {
    case wintype_AllTypes:
        glk_stylehint_set(wintype_TextBuffer, styl, hint, val);
        glk_stylehint_set(wintype_TextGrid, styl, hint, val);
        return;
    case wintype_TextBuffer:
        stylehint = &textbuffer_stylehints[styl];
        break;
    case wintype_TextGrid:
        stylehint = &textgrid_stylehints[styl];
        break;
    default:
        return;
    }

    switch (hint) {
    case stylehint_Weight:
        if (val >= -1 && val <= 1) {
            stylehint->weight = val;
        }
        break;
    case stylehint_Oblique:
        if (val >= 0 && val <= 1) {
            stylehint->oblique = val;
        }
        break;
    case stylehint_TextColor:
        if (val >= 0 && val <= 0xFFFFFF) {
            stylehint->textcolor = val;
            stylehint->textcolori = get_nearest_curses_color(val);
        }
        break;
    case stylehint_BackColor:
        if (val >= 0 && val <= 0xFFFFFF) {
            stylehint->backcolor = val;
            stylehint->backcolori = get_nearest_curses_color(val);
        }
        break;
    case stylehint_ReverseColor:
        if (val >= 0 && val <= 1) {
            stylehint->reversecolor = val;
        }
        break;
    }
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
    stylehint_t *stylehint = NULL;

    if (!pref_stylehint ||
        styl >= style_NUMSTYLES || hint >= stylehint_NUMHINTS) {
        return;
    }

    switch (wintype) {
    case wintype_AllTypes:
        glk_stylehint_clear(wintype_TextBuffer, styl, hint);
        glk_stylehint_clear(wintype_TextGrid, styl, hint);
        return;
    case wintype_TextBuffer:
        stylehint = &textbuffer_stylehints[styl];
        break;
    case wintype_TextGrid:
        stylehint = &textgrid_stylehints[styl];
        break;
    default:
        return;
    }

    switch (hint) {
    case stylehint_Weight:
        stylehint->weight = 0;
        break;
    case stylehint_Oblique:
        stylehint->oblique = 0;
        break;
    case stylehint_TextColor:
        stylehint->textcolor = -1;
        stylehint->textcolori = -1;
        break;
    case stylehint_BackColor:
        stylehint->backcolor = -1;
        stylehint->backcolori = -1;
        break;
    case stylehint_ReverseColor:
        stylehint->reversecolor = pref_reverse_textgrids ? 1 : 0;
        break;
    }
}

glui32 glk_style_distinguish(window_t *win, glui32 styl1, glui32 styl2)
{
    const stylehint_t *stylehint1, *stylehint2;

    if (!win) {
        gli_strict_warning("style_distinguish: invalid ref");
        return FALSE;
    }
    
    if (styl1 >= style_NUMSTYLES || styl2 >= style_NUMSTYLES)
        return FALSE;

    stylehint1 = &win->stylehints[styl1];
    stylehint2 = &win->stylehints[styl2];
    if (stylehint1->weight != stylehint2->weight ||
        stylehint1->oblique != stylehint2->oblique ||
        stylehint1->reversecolor != stylehint2->reversecolor) {
        return TRUE;
    }
    if (stylehint1->textcolori != stylehint2->textcolori) {
        return TRUE;
    }
    if (stylehint1->backcolori != stylehint2->backcolori) {
        return TRUE;
    }
    
    return FALSE;
}

glui32 glk_style_measure(window_t *win, glui32 styl, glui32 hint, 
    glui32 *result)
{
    glui32 dummy;

    if (!win) {
        gli_strict_warning("style_measure: invalid ref");
        return FALSE;
    }
    
    if (styl >= style_NUMSTYLES || hint >= stylehint_NUMHINTS)
        return FALSE;
    
    if (!result)
        result = &dummy;
    
    switch (hint) {
        case stylehint_Indentation:
        case stylehint_ParaIndentation:
            *result = 0;
            return TRUE;
        case stylehint_Justification:
            *result = stylehint_just_LeftFlush ? 1 : 0;
            return TRUE;
        case stylehint_Size:
            *result = 1;
            return TRUE;
        case stylehint_Weight:
            *result = win->stylehints[styl].weight;
            return TRUE;
        case stylehint_Oblique:
            *result = win->stylehints[styl].oblique;
            return TRUE;
        case stylehint_Proportional:
            *result = FALSE;
            return TRUE;
        case stylehint_TextColor:
            *result = win->stylehints[styl].textcolor;
            return TRUE;
        case stylehint_BackColor:
            *result = win->stylehints[styl].backcolor;
            return TRUE;
        case stylehint_ReverseColor:
            /* This does not look at inline_reverse. */
            *result = win->stylehints[styl].reversecolor;
            return TRUE;
    }
    
    return FALSE;
}
