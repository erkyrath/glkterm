/* gtw_grid.c: The grid window type
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#define _XOPEN_SOURCE /* wcwidth */
#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <wchar.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_grid.h"

/* A grid of characters. We store the window as a list of lines (see
    gtw_grid.h); within a line, just store an array of characters and
    an array of styles, the same size.
*/

static void init_lines(window_textgrid_t *dwin, int beg, int end, int linewid);
static void final_lines(window_textgrid_t *dwin);
static void export_input_line(void *buf, int unicode, long len, wchar_t *chars);
static void import_input_line(tgline_t *ln, int offset, void *buf, 
    int unicode, long len);

/* Array of curses.h attribute values, one for each style. */
int win_textgrid_styleattrs[style_NUMSTYLES];

/* This macro sets the appropriate dirty values, when a single character
    (at px, py) is changed. */
#define setposdirty(dwn, ll, px, py)   \
    if (dwn->dirtybeg == -1 || (py) < dwn->dirtybeg)   \
        dwn->dirtybeg = (py);   \
    if (dwn->dirtyend == -1 || (py)+1 > dwn->dirtyend)   \
        dwn->dirtyend = (py)+1;   \
    if (ll->dirtybeg == -1 || (px) < ll->dirtybeg)   \
        ll->dirtybeg = (px);   \
    if (ll->dirtyend == -1 || (px)+1 > ll->dirtyend)   \
        ll->dirtyend = (px)+1;   \
    

/* lnoffset could be made inline if the compiler supports it */
int lnoffset(tgline_t *ln, int pos)
{
    /* N.B. ln->size only gives us protection against buffer overflow
     * since we don't have access to width, invalid pos will give an
     * invalid return value.
     */
    int x = wcwidth(ln->chars[0]);
    int i = 0;
    
    while ( x <= pos && i < ln->size ) {
        if ( ++i < ln->size )
            x += wcwidth(ln->chars[i]);
    }
    
    return i;
}

window_textgrid_t *win_textgrid_create(window_t *win)
{
    window_textgrid_t *dwin = (window_textgrid_t *)malloc(sizeof(window_textgrid_t));
    dwin->owner = win;
    
    dwin->width = 0;
    dwin->height = 0;
    
    dwin->curx = 0;
    dwin->cury = 0;
    
    dwin->linessize = 0;
    dwin->lines = NULL;
    dwin->dirtybeg = -1;
    dwin->dirtyend = -1;
    
    dwin->inbuf = NULL;
    dwin->inunicode = FALSE;
    dwin->inorgx = 0;
    dwin->inorgy = 0;
    
    return dwin;
}

void win_textgrid_destroy(window_textgrid_t *dwin)
{
    if (dwin->inbuf) {
        if (gli_unregister_arr) {
            char *typedesc = (dwin->inunicode ? "&+#!Iu" : "&+#!Cn");
            (*gli_unregister_arr)(dwin->inbuf, dwin->inoriglen, typedesc, dwin->inarrayrock);
        }
        dwin->inbuf = NULL;
    }
    
    dwin->owner = NULL;
    if (dwin->lines) {
        final_lines(dwin);
    }
    free(dwin);
}

void win_textgrid_rearrange(window_t *win, grect_t *box)
{
    int ix, jx, oldval;
    int newwid, newhgt;
    window_textgrid_t *dwin = win->data;
    dwin->owner->bbox = *box;
    
    newwid = box->right - box->left;
    newhgt = box->bottom - box->top;
    
    if (dwin->lines == NULL) {
        dwin->linessize = (newhgt+1);
        dwin->lines = (tgline_t *)malloc(dwin->linessize * sizeof(tgline_t));
        if (!dwin->lines)
            return;
        init_lines(dwin, 0, dwin->linessize, newwid);
    }
    else {
        if (newhgt > dwin->linessize) {
            oldval = dwin->linessize;
            dwin->linessize = (newhgt+1) * 2;
            dwin->lines = (tgline_t *)realloc(dwin->lines, 
                dwin->linessize * sizeof(tgline_t));
            if (!dwin->lines)
                return;
            init_lines(dwin, oldval, dwin->linessize, newwid);
        }
        if (newhgt > dwin->height) {
            for (jx=dwin->height; jx<newhgt; jx++) {
                tgline_t *ln = &(dwin->lines[jx]);
                for (ix=0; ix<ln->size; ix++) {
                    ln->chars[ix] = L' ';
                    ln->attrs[ix] = style_Normal;
                }
            }
        }
        for (jx=0; jx<newhgt; jx++) {
            tgline_t *ln = &(dwin->lines[jx]);
            if (newwid > ln->size) {
                oldval = ln->size;
                ln->size = (newwid+1) * 2;
                ln->chars = (wchar_t *)realloc(ln->chars, 
                    ln->size * sizeof(wchar_t));
                ln->attrs = (short *)realloc(ln->attrs, 
                    ln->size * sizeof(short));
                if (!ln->chars || !ln->attrs) {
                    dwin->lines = NULL;
                    return;
                }
                for (ix=oldval; ix<ln->size; ix++) {
                    ln->chars[ix] = L' ';
                    ln->attrs[ix] = style_Normal;
                }
            }
        }
    }
    
    dwin->width = newwid;
    dwin->height = newhgt;

    dwin->dirtybeg = 0;
    dwin->dirtyend = dwin->height;
}

static void init_lines(window_textgrid_t *dwin, int beg, int end, int linewid)
{
    int ix, jx;

    for (jx=beg; jx<end; jx++) {
        tgline_t *ln = &(dwin->lines[jx]);
        ln->size = (linewid+1);
        ln->dirtybeg = -1;
        ln->dirtyend = -1;
        ln->chars = (wchar_t *)malloc(ln->size * sizeof(wchar_t));
        ln->attrs = (short *)malloc(ln->size * sizeof(short));
        if (!ln->chars || !ln->size) {
            dwin->lines = NULL;
            return;
        }
        for (ix=0; ix<ln->size; ix++) {
            ln->chars[ix] = L' ';
            ln->attrs[ix] = style_Normal;
        }
    }
}

static void final_lines(window_textgrid_t *dwin)
{
    int jx;
    
    for (jx=0; jx<dwin->linessize; jx++) {
        tgline_t *ln = &(dwin->lines[jx]);
        if (ln->chars) {
            free(ln->chars);
            ln->chars = NULL;
        }
        if (ln->attrs) {
            free(ln->attrs);
            ln->attrs = NULL;
        }
    }
    
    free(dwin->lines);
    dwin->lines = NULL;
}

static void updatetext(window_textgrid_t *dwin, int drawall)
{
    int ix, jx, beg;
    int orgx, orgy;
    short curattr;
    
    if (drawall) {
        dwin->dirtybeg = 0;
        dwin->dirtyend = dwin->height;
    }
    else {
        if (dwin->dirtyend > dwin->height) {
            dwin->dirtyend = dwin->height;
        }
    }
    
    if (dwin->dirtybeg == -1)
        return;
    
    orgx = dwin->owner->bbox.left;
    orgy = dwin->owner->bbox.top;
    
    for (jx=dwin->dirtybeg; jx<dwin->dirtyend; jx++) {
        tgline_t *ln = &(dwin->lines[jx]);
        if (drawall) {
            ln->dirtybeg = 0;
            ln->dirtyend = dwin->width;
        }
        else {
            if (ln->dirtyend > dwin->width) {
                ln->dirtyend = dwin->width;
            }
        }

        if (ln->dirtybeg == -1)
            continue;
        
        /* draw one line. */
        move(orgy+jx, orgx+ln->dirtybeg);
        
        ix=ln->dirtybeg;
        while (ix<ln->dirtyend) {
            wchar_t *ucx;
            beg = ix;
            curattr = ln->attrs[lnoffset(ln, beg)];
            for (ix+=wcwidth(ln->chars[lnoffset(ln, ix)]); ix<ln->dirtyend && ln->attrs[lnoffset(ln, ix)] == curattr; ix+=wcwidth(ln->chars[lnoffset(ln, ix)])) { }
            attrset(win_textgrid_styleattrs[curattr]);
            ucx = ln->chars;
            local_addnwstr(ucx + lnoffset(ln, beg), lnoffset(ln, ix) - lnoffset(ln, beg));
        }
        
        ln->dirtybeg = -1;
        ln->dirtyend = -1;
    }
    
    attrset(0);
    
    dwin->dirtybeg = -1;
    dwin->dirtyend = -1;
}

void win_textgrid_redraw(window_t *win)
{
    window_textgrid_t *dwin = win->data;

    if (!dwin->lines)
        return;
    
    updatetext(dwin, TRUE);
}

void win_textgrid_update(window_t *win)
{
    window_textgrid_t *dwin = win->data;

    if (!dwin->lines)
        return;
    
    updatetext(dwin, FALSE);
}

void win_textgrid_putchar(window_t *win, wchar_t ch)
{
    window_textgrid_t *dwin = win->data;
    tgline_t *ln;
    size_t ch_width = wcwidth(ch);

    
    /* Canonicalize the cursor position. That is, the cursor may have been
        left outside the window area, or may be too close to the edge to print
        the next character; wrap it if necessary. */
    if (dwin->curx < 0)
        dwin->curx = 0;
    else if (dwin->curx > 0 && dwin->curx + ch_width > dwin->width) {
        dwin->curx = 0;
        dwin->cury++;
    }
    if (dwin->cury < 0)
        dwin->cury = 0;
    else if (dwin->cury >= dwin->height)
        return; /* outside the window */
    
    if (ch == L'\n') {
        /* a newline just moves the cursor. */
        dwin->cury++;
        dwin->curx = 0;
        return;
    }
    
    ln = &(dwin->lines[dwin->cury]);
    
    /* We will use this repeatedly: */
    int curx_offset = lnoffset(ln, dwin->curx);
    
    /* What if we overlap with one or more 2-glyph characters? */
    /* N.B. we are assuming 2-glyph here.  We really should handle arbitrary glyph width */
    
    /* Test for overlapping with the second half of a 2-glyph character */
    if ( dwin->curx > 0 && lnoffset(ln, dwin->curx - 1) == curx_offset ) {
        /* Shift rest of line buffer 1 cell to the right */
        /* We don't have to check for memory boundaries because the 2-glyph 
         * character we are overlapping guarantees us that we are not using
         * the entire chars[] buffer.
         */
        memmove(ln->chars + curx_offset + 2, ln->chars + curx_offset + 1, (ln->size - curx_offset - 1) * sizeof(wchar_t));
            /* obliterate the previous half-character */
        /* N.B. This effectively changes the value of lnoffset(ln, dwin->curx) */
        ln->chars[curx_offset++] = L'?';
        /* obliterate target cell, to make calculations below consistent */
        ln->chars[curx_offset] = L'?';
        setposdirty(dwin, ln, dwin->curx - 1, dwin->cury);
    }

    size_t target_width = wcwidth(ln->chars[curx_offset]);

    /* Test for overlapping with the first half of a 2-glyph character */
    /* N.B. Because we have already dealt with any overlaps with a previous character
     * we know that we start on a character boundary
     */
    /* N.B. the memmoves below are exclusive cases to the memmove above */
    if ( target_width < ch_width ) {
        /* We can't fit this character in the needed (grid) space. */
        if ( wcwidth(ln->chars[curx_offset + 1]) > 1 ) {
            /* Next character is wide, so it will become garbage. */
            ln->chars[curx_offset + 1] = L'?';
            setposdirty(dwin, ln, dwin->curx + ch_width, dwin->cury);
        }
	else {
            /* Next character is narrow, so we'll cover it entirely. */
            memmove(ln->chars + curx_offset + 1, ln->chars + curx_offset + 2, (ln->size - curx_offset - 2) * sizeof(wchar_t));
            /* We don't need to fill in ln->chars[ln->width - 1], because it will never get printed. */
        }
    }
    else if ( target_width > ch_width ) {
        /* This character can't fill the space we are filling. */
        /* Insert a dummy cell after this character. */
        memmove(ln->chars + curx_offset + 2, ln->chars + curx_offset + 1, (ln->size - curx_offset - 2) * sizeof(wchar_t));
        /* Set next character to ? */
        ln->chars[curx_offset + 1] = L'?';
        setposdirty(dwin, ln, dwin->curx + ch_width, dwin->cury);
    }
    
    ln->chars[curx_offset] = ch;
    ln->attrs[curx_offset] = win->style;
    setposdirty(dwin, ln, dwin->curx, dwin->cury);
    if ( ch_width > 1 )
        setposdirty(dwin, ln, dwin->curx + 1, dwin->cury);
        
    dwin->curx += ch_width;
    
    /* We can leave the cursor outside the window, since it will be
        canonicalized next time a character is printed. */
}

void win_textgrid_clear(window_t *win)
{
    int ix, jx;
    window_textgrid_t *dwin = win->data;
    
    for (jx=0; jx<dwin->height; jx++) {
        tgline_t *ln = &(dwin->lines[jx]);
        for (ix=0; ix<dwin->width; ix++) {
            ln->chars[ix] = L' ';
            ln->attrs[ix] = style_Normal;
        }
        ln->dirtybeg = 0;
        ln->dirtyend = dwin->width;
    }

    dwin->dirtybeg = 0;
    dwin->dirtyend = dwin->height;
    
    dwin->curx = 0;
    dwin->cury = 0;
}

void win_textgrid_move_cursor(window_t *win, int xpos, int ypos)
{
    window_textgrid_t *dwin = win->data;
    
    /* If the values are negative, they're really huge positive numbers -- 
        remember that they were cast from glui32. So set them huge and
        let canonicalization take its course. */
    if (xpos < 0)
        xpos = 32767;
    if (ypos < 0)
        ypos = 32767;
        
    dwin->curx = xpos;
    dwin->cury = ypos;
}

void win_textgrid_place_cursor(window_t *win, int *xpos, int *ypos)
{
    window_textgrid_t *dwin = win->data;
    
    /* Canonicalize the cursor position */
    if (dwin->curx < 0)
        dwin->curx = 0;
    else if (dwin->curx >= dwin->width) {
        dwin->curx = 0;
        dwin->cury++;
    }
    if (dwin->cury < 0)
        dwin->cury = 0;
    else if (dwin->cury >= dwin->height) {
        *xpos = dwin->width-1;
        *ypos = dwin->height-1;
        return;
    }
    
    *xpos = dwin->curx;
    *ypos = dwin->cury;
}

/* Prepare the window for line input. */
void win_textgrid_init_line(window_t *win, void *buf, int unicode,
    int maxlen, int initlen)
{
    window_textgrid_t *dwin = win->data;
    
    dwin->inbuf = buf;
    dwin->inunicode = unicode;
    dwin->inoriglen = maxlen;
    if (maxlen > (dwin->width - dwin->curx))
        maxlen = (dwin->width - dwin->curx);
    dwin->inmax = maxlen;
    dwin->inlen = 0;
    dwin->incurs = 0;
    dwin->inorgx = dwin->curx;
    dwin->inorgy = dwin->cury;
    dwin->origstyle = win->style;
    win->style = style_Input;
    
    if (initlen > maxlen)
        initlen = maxlen;
        
    if (initlen) {
        tgline_t *ln = &(dwin->lines[dwin->inorgy]);

        if (initlen) {
            import_input_line(ln, dwin->inorgx, dwin->inbuf, 
                dwin->inunicode, initlen);
        }        
        
        setposdirty(dwin, ln, dwin->inorgx+0, dwin->inorgy);
        if (initlen > 1) {
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+initlen-1), dwin->inorgy);
        }
            
        dwin->incurs += initlen;
        dwin->inlen += initlen;
        dwin->curx = wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs);
        dwin->cury = dwin->inorgy;
    }

    if (gli_register_arr) {
        char *typedesc = (dwin->inunicode ? "&+#!Iu" : "&+#!Cn");
        dwin->inarrayrock = (*gli_register_arr)(dwin->inbuf, dwin->inoriglen, typedesc);
    }
}

/* Abort line input, storing whatever's been typed so far. */
void win_textgrid_cancel_line(window_t *win, event_t *ev)
{
    void *inbuf;
    int inoriglen, inmax, inunicode;
    gidispatch_rock_t inarrayrock;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (!dwin->inbuf)
        return;
    
    inbuf = dwin->inbuf;
    inmax = dwin->inmax;
    inoriglen = dwin->inoriglen;
    inarrayrock = dwin->inarrayrock;
    inunicode = dwin->inunicode;

    export_input_line(inbuf, inunicode, dwin->inlen, &ln->chars[dwin->inorgx]);

    if (win->echostr) {
        if (!inunicode)
            gli_stream_echo_line(win->echostr, inbuf, dwin->inlen);
        else
            gli_stream_echo_line_uni(win->echostr, inbuf, dwin->inlen);
    }

    dwin->cury = dwin->inorgy+1;
    dwin->curx = 0;
    win->style = dwin->origstyle;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = dwin->inlen;
    
    win->line_request = FALSE;
    dwin->inbuf = NULL;
    dwin->inoriglen = 0;
    dwin->inmax = 0;
    dwin->inorgx = 0;
    dwin->inorgy = 0;

    if (gli_unregister_arr) {
        char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inoriglen, typedesc, inarrayrock);
    }
}

static void import_input_line(tgline_t *ln, int offset, void *buf, 
    int unicode, long len)
{
    int ix;

    if (!unicode) {
        for (ix=0; ix<len; ix++) {
            char ch = ((char *)buf)[ix];
            ln->attrs[offset+ix] = style_Input;
            ln->chars[offset+ix] = UCS(ch);
        }
    }
    else {
        for (ix=0; ix<len; ix++) {
            glui32 kval = ((glui32 *)buf)[ix];
            ln->attrs[offset+ix] = style_Input;
            ln->chars[offset+ix] = kval;
        }
    }
}

/* Clone in gtw_buf.c */
static void export_input_line(void *buf, int unicode, long len, wchar_t *chars)
{
    int ix;

    if (!unicode) {
        for (ix=0; ix<len; ix++) {
            wchar_t val = chars[ix];
            glui32 kval = gli_input_from_native(val);
            ((char *)buf)[ix] = Lat(kval);
        }
    }
    else {
        for (ix=0; ix<len; ix++) {
            wchar_t val = chars[ix];
            glui32 kval = gli_input_from_native(val);
            ((glui32 *)buf)[ix] = kval;
        }
    }
}

/* Keybinding functions. */

/* Any key, during character input. Ends character input. */
void gcmd_grid_accept_key(window_t *win, glui32 arg)
{
    win->char_request = FALSE; 
    arg = gli_input_from_native(arg);
    gli_event_store(evtype_CharInput, win, arg, 0);
}

/* Return or enter, during line input. Ends line input. */
void gcmd_grid_accept_line(window_t *win, glui32 arg)
{
    void *inbuf;
    int inoriglen, inmax, inunicode;
    gidispatch_rock_t inarrayrock;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);
    
    if (!dwin->inbuf)
        return;
    
    inbuf = dwin->inbuf;
    inmax = dwin->inmax;
    inoriglen = dwin->inoriglen;
    inarrayrock = dwin->inarrayrock;
    inunicode = dwin->inunicode;

    export_input_line(inbuf, inunicode, dwin->inlen, &ln->chars[dwin->inorgx]);

    if (win->echostr) {
        if (!inunicode)
            gli_stream_echo_line(win->echostr, inbuf, dwin->inlen);
        else
            gli_stream_echo_line_uni(win->echostr, inbuf, dwin->inlen);
    }

    dwin->cury = dwin->inorgy+1;
    dwin->curx = 0;
    win->style = dwin->origstyle;

    gli_event_store(evtype_LineInput, win, dwin->inlen, 0);
    win->line_request = FALSE;
    dwin->inbuf = NULL;
    dwin->inoriglen = 0;
    dwin->inmax = 0;
    dwin->inorgx = 0;
    dwin->inorgy = 0;

    if (gli_unregister_arr) {
        char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inoriglen, typedesc, inarrayrock);
    }
}

/* Any regular key, during line input. */
void gcmd_grid_insert_key(window_t *win, glui32 arg)
{
    int ix;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);
    
    if (!dwin->inbuf)
        return;
    if (dwin->inlen >= dwin->inmax)
        return;
    
    /* N.B. incurs is a buffer offset. */
    for (ix=dwin->inlen; ix>dwin->incurs; ix--) 
        ln->chars[lnoffset(ln, dwin->inorgx)+ix] = ln->chars[lnoffset(ln, dwin->inorgx)+ix-1];
    ln->attrs[lnoffset(ln, dwin->inorgx)+dwin->inlen] = style_Input;
    ln->chars[lnoffset(ln, dwin->inorgx)+dwin->incurs] = glui32_to_wchar(arg);
    
    setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs), dwin->inorgy);
    if (dwin->incurs != dwin->inlen) {
        setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->inlen), dwin->inorgy);
    }
    
    dwin->incurs++;
    dwin->inlen++;
    dwin->curx = wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs);
    dwin->cury = dwin->inorgy;
    
    updatetext(dwin, FALSE);
}

/* Delete keys, during line input. */
void gcmd_grid_delete(window_t *win, glui32 arg)
{
    int ix;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);
    
    if (!dwin->inbuf)
        return;
    
    if (dwin->inlen <= 0)
        return;
                
    switch (arg) {
        case gcmd_Delete:
            if (dwin->incurs <= 0)
                return;
            for (ix=dwin->incurs; ix<dwin->inlen; ix++) 
                ln->chars[lnoffset(ln, dwin->inorgx)+ix-1] = ln->chars[lnoffset(ln, dwin->inorgx)+ix];
            ln->chars[lnoffset(ln, dwin->inorgx)+dwin->inlen-1] = L' ';
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs-1), dwin->inorgy);
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->inlen-1), dwin->inorgy);
            dwin->incurs--;
            dwin->inlen--;
            break;
        case gcmd_DeleteNext:
            if (dwin->incurs >= dwin->inlen)
                return;
            for (ix=dwin->incurs; ix<dwin->inlen-1; ix++) 
                ln->chars[lnoffset(ln, dwin->inorgx)+ix] = ln->chars[lnoffset(ln, dwin->inorgx)+ix+1];
            ln->chars[dwin->inorgx+dwin->inlen-1] = L' ';
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs), dwin->inorgy);
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->inlen-1), dwin->inorgy);
            dwin->inlen--;
            break;
        case gcmd_KillInput:
            for (ix=0; ix<dwin->inlen; ix++) 
                ln->chars[lnoffset(ln, dwin->inorgx)+ix] = L' ';
            setposdirty(dwin, ln, dwin->inorgx+0, dwin->inorgy);
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->inlen-1), dwin->inorgy);
            dwin->inlen = 0;
            dwin->incurs = 0;
            break;
        case gcmd_KillLine:
            if (dwin->incurs >= dwin->inlen)
                return;
            for (ix=dwin->incurs; ix<dwin->inlen; ix++) 
                ln->chars[lnoffset(ln, dwin->inorgx)+ix] = L' ';
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs), dwin->inorgy);
            setposdirty(dwin, ln, wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->inlen-1), dwin->inorgy);
            dwin->inlen = dwin->incurs;
            break;
    }

    dwin->curx = wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs);
    dwin->cury = dwin->inorgy;
    
    updatetext(dwin, FALSE);
}

/* Cursor movement keys, during line input. */
void gcmd_grid_move_cursor(window_t *win, glui32 arg)
{
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);
    
    if (!dwin->inbuf)
        return;

    switch (arg) {
        case gcmd_Left:
            if (dwin->incurs <= 0)
                return;
            dwin->incurs--;
            break;
        case gcmd_Right:
            if (dwin->incurs >= dwin->inlen)
                return;
            dwin->incurs++;
            break;
        case gcmd_LeftEnd:
            if (dwin->incurs <= 0)
                return;
            dwin->incurs = 0;
            break;
        case gcmd_RightEnd:
            if (dwin->incurs >= dwin->inlen)
                return;
            dwin->incurs = dwin->inlen;
            break;
    }

    dwin->curx = wcswidth(ln->chars, lnoffset(ln, dwin->inorgx)+dwin->incurs);
    dwin->cury = dwin->inorgy;
    
}
