/* gtw_buf.c: The buffer window type
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_buf.h"

/* Array of curses.h attribute values, one for each style. */
chtype win_textbuffer_styleattrs[style_NUMSTYLES];

static void final_lines(window_textbuffer_t *dwin, long beg, long end);
static long find_style_by_pos(window_textbuffer_t *dwin, long pos);
static long find_line_by_pos(window_textbuffer_t *dwin, long pos);
static void set_last_run(window_textbuffer_t *dwin, glui32 style);

window_textbuffer_t *win_textbuffer_create(window_t *win)
{
    window_textbuffer_t *dwin = (window_textbuffer_t *)malloc(sizeof(window_textbuffer_t));
    dwin->owner = win;
    
    dwin->numchars = 0;
    dwin->charssize = 500;
    dwin->chars = (char *)malloc(dwin->charssize * sizeof(char));
    
    dwin->numlines = 0;
    dwin->linessize = 50;
    dwin->lines = (tbline_t *)malloc(dwin->linessize * sizeof(tbline_t));
    
    dwin->numruns = 0;
    dwin->runssize = 40;
    dwin->runs = (tbrun_t *)malloc(dwin->runssize * sizeof(tbrun_t));
    
    dwin->tmplinessize = 40;
    dwin->tmplines = (tbline_t *)malloc(dwin->tmplinessize * sizeof(tbline_t));
    
    dwin->tmpwordssize = 40;
    dwin->tmpwords = (tbword_t *)malloc(dwin->tmpwordssize * sizeof(tbword_t));
    
    if (!dwin->chars || !dwin->runs || !dwin->lines 
        || !dwin->tmplines || !dwin->tmpwords)
        return NULL;
    
    dwin->numruns = 1;
    dwin->runs[0].style = style_Normal;
    dwin->runs[0].pos = 0;
    
    dwin->dirtybeg = -1;
    dwin->dirtyend = -1;
    dwin->dirtydelta = -1;
    dwin->scrollline = 0;
    dwin->scrollpos = 0;
    dwin->drawall = TRUE;
    
    dwin->width = -1;
    dwin->height = -1;

    return dwin;
}

void win_textbuffer_destroy(window_textbuffer_t *dwin)
{
    dwin->owner = NULL;
    
    if (dwin->tmplines) {
        /* don't try to destroy tmplines; they're all invalid */
        free(dwin->tmplines);
        dwin->tmplines = NULL;
    }
    
    if (dwin->lines) {
        final_lines(dwin, 0, dwin->numlines);
        free(dwin->lines);
        dwin->lines = NULL;
    }
    
    if (dwin->runs) {
        free(dwin->runs);
        dwin->runs = NULL;
    }
    
    if (dwin->chars) {
        free(dwin->chars);
        dwin->chars = NULL;
    }
    
    free(dwin);
}

static void final_lines(window_textbuffer_t *dwin, long beg, long end)
{
    long lx;
    
    for (lx=beg; lx<end; lx++) {
        tbline_t *ln = &(dwin->lines[lx]);
        if (ln->words) {
            free(ln->words);
            ln->words = NULL;
        }
    }
}

void win_textbuffer_rearrange(window_t *win, grect_t *box)
{
    int oldwid, oldhgt;
    window_textbuffer_t *dwin = win->data;
    dwin->owner->bbox = *box;

    oldwid = dwin->width;
    oldhgt = dwin->height;

    dwin->width = box->right - box->left;
    dwin->height = box->bottom - box->top;
    
    if (oldwid != dwin->width) {
        /* Set dirty region to the whole (or visible?), and
            delta should indicate that the whole old region is changed. */
        if (dwin->dirtybeg == -1) {
            dwin->dirtybeg = 0;
            dwin->dirtyend = dwin->numchars;
            dwin->dirtydelta = 0;
        }
        else {
            dwin->dirtybeg = 0;
            dwin->dirtyend = dwin->numchars;
        }
    }
    else if (oldhgt != dwin->height) {
    }
}

/* Find the last line for which pos >= line.pos. If pos is before the first 
    line.pos, or if there are no lines, this returns -1. Lines always go to
    the end of the text, so this will never be numlines or higher. */
static long find_line_by_pos(window_textbuffer_t *dwin, long pos)
{
    long lx;
    
    if (dwin->numlines == 0)
        return -1;
    
    for (lx=dwin->numlines-1; lx >= 0; lx--) {
        if (pos >= dwin->lines[lx].pos)
            return lx;
    }
    
    return -1;
}

/* Find the last stylerun for which pos >= style.pos. We know run[0].pos == 0,
    so the result is always >= 0. */
static long find_style_by_pos(window_textbuffer_t *dwin, long pos)
{
    long beg, end, val;
    tbrun_t *runs = dwin->runs;
    
    /* Do a binary search, maintaining 
            runs[beg].pos <= pos < runs[end].pos
        (we pretend that runs[numruns].pos is infinity) */
    
    beg = 0;
    end = dwin->numruns;
    
    while (beg+1 < end) {
        val = (beg+end) / 2;
        if (pos >= runs[val].pos) {
            beg = val;
        }
        else {
            end = val;
        }
    }
    
    return beg;
}

/* This does layout on a segment of text, writing into tmplines. Returns
    the number of lines laid. Assumes tmplines is entirely unused, 
    initially. */
static long layout_chars(window_textbuffer_t *dwin, long chbeg, long chend,
    int startpara)
{
    long cx, cx2, lx, rx;
    long numwords; 
    long linestartpos;
    char ch;
    int lastlinetype;
    short style;
    long styleendpos;
    /* cache some values */
    char *chars = dwin->chars;
    tbrun_t *runs = dwin->runs;
    
    lastlinetype = (startpara) ? wd_EndLine : wd_Text;
    cx = chbeg;
    linestartpos = chbeg;
    lx = 0;
    numwords = 0; /* actually number of tmpwords */
    
    rx = find_style_by_pos(dwin, chbeg);
    style = runs[rx].style;
    if (rx+1 >= dwin->numruns)
        styleendpos = chend+1;
    else
        styleendpos = runs[rx+1].pos;
    
    /* create lines until we reach the end of the text segment; but if the 
        last line ends with a newline, go one more. */
    
    while (numwords || cx < chend || lastlinetype != wd_EndPage) {
        tbline_t *ln;
        int lineover, lineeatto, lastsolidwd;
        long lineeatpos;
        long wx, wx2;
        int linewidth, widthsofar;
        int linetype;
        
        if (lx+2 >= dwin->tmplinessize) {
            /* leaves room for a final line */
            dwin->tmplinessize *= 2;
            dwin->tmplines = (tbline_t *)realloc(dwin->tmplines, 
                dwin->tmplinessize * sizeof(tbline_t));
        }
        ln = &(dwin->tmplines[lx]);
        lx++;
        
        lineover = FALSE;
        lineeatto = 0;
        lastsolidwd = 0;
        widthsofar = 0;
        linewidth = dwin->width;
        
        wx = 0;
        
        while ((wx < numwords || cx < chend) && !lineover) {
            tbword_t *wd;
            
            if (wx >= numwords) {
                /* suck down a new word. */
                
                if (wx+2 >= dwin->tmpwordssize) {
                    /* leaves room for a split word */
                    dwin->tmpwordssize *= 2;
                    dwin->tmpwords = (tbword_t *)realloc(dwin->tmpwords, 
                        dwin->tmpwordssize * sizeof(tbword_t));
                }
                wd = &(dwin->tmpwords[wx]);
                wx++;
                numwords++;
                
                ch = chars[cx];
                cx2 = cx;
                cx++;
                if (ch == '\n') {
                    wd->type = wd_EndLine;
                    wd->pos = cx2;
                    wd->len = 0;
                    wd->style = style;
                }
                else if (ch == ' ') {
                    wd->type = wd_Blank;
                    wd->pos = cx2;
                    while (cx < chend 
                            && cx < styleendpos && chars[cx] == ' ')
                        cx++;
                    wd->len = cx - (wd->pos);
                    wd->style = style;
                }
                else {
                    wd->type = wd_Text;
                    wd->pos = cx2;
                    while (cx < chend 
                            && cx < styleendpos && chars[cx] != '\n' 
                            && chars[cx] != ' ')
                        cx++;
                    wd->len = cx - (wd->pos);
                    wd->style = style;
                }
                
                if (cx >= styleendpos) {
                    rx++;
                    style = runs[rx].style;
                    if (rx+1 >= dwin->numruns)
                        styleendpos = chend+1;
                    else
                        styleendpos = runs[rx+1].pos;
                }
            }
            else {
                /* pull out an existing word. */
                wd = &(dwin->tmpwords[wx]);
                wx++;
            }
            
            if (wd->type == wd_EndLine) {
                lineover = TRUE;
                lineeatto = wx;
                lineeatpos = wd->pos+1;
                linetype = wd_EndLine;
            }
            else {
                if (wd->type == wd_Blank 
                        || widthsofar + wd->len <= linewidth) {
                    widthsofar += wd->len;
                }
                else {
                    /* last text word goes over. */
                    for (wx2 = wx-1; 
                        wx2 > 0 && dwin->tmpwords[wx2-1].type == wd_Text; 
                        wx2--) { }
                    /* wx2 is now the first text word of this group, which 
                        is to say right after the last blank word. If this
                        is > 0, chop there; otherwise we have to split a
                        word somewhere. */
                    if (wx2 > 0) {
                        lineover = TRUE;
                        lineeatto = wx2;
                        lineeatpos = dwin->tmpwords[wx2].pos;
                        linetype = wd_Text;
                    }
                    else {
                        /* first group goes over; gotta split. But we know
                            the last word of the group is the culprit. */
                        int extra = widthsofar + wd->len - linewidth;
                        /* extra is the amount hanging outside the boundary; 
                            will be > 0. */
                        if (wd->len == extra) {
                            /* the whole last word is hanging out. Just 
                                chop. */
                            lineover = TRUE;
                            lineeatto = wx-1;
                            lineeatpos = wd->pos;
                            linetype = wd_Text;
                        }
                        else {
                            /* split the last word, creating a new one. */
                            tbword_t *wd2;
                            if (wx < numwords) {
                                memmove(dwin->tmpwords+(wx+1), 
                                    dwin->tmpwords+wx, 
                                    (numwords-wx) * sizeof(tbword_t));
                            }
                            wd2 = &(dwin->tmpwords[wx]);
                            wx++;
                            numwords++;
                            wd->len -= extra;
                            wd2->type = wd->type;
                            wd2->style = wd->style;
                            wd2->pos = wd->pos+wd->len;
                            wd2->len = extra;
                            lineover = TRUE;
                            lineeatto = wx-1;
                            lineeatpos = wd2->pos;
                            linetype = wd_Text;
                        }
                    }
                }
            }
        }
        
        if (!lineover) {
            /* ran out of characters, no newline */
            lineeatto = wx;
            lineeatpos = chend;
            linetype = wd_EndPage;
        }
        
        if (lineeatto) {
            ln->words = (tbword_t *)malloc(lineeatto * sizeof(tbword_t));
            memcpy(ln->words, dwin->tmpwords, lineeatto * sizeof(tbword_t));
            ln->numwords = lineeatto;
            
            if (lineeatto < numwords) {
                memmove(dwin->tmpwords, 
                    dwin->tmpwords+lineeatto, 
                    (numwords-lineeatto) * sizeof(tbword_t));
            }
            numwords -= lineeatto;
        }
        else {
            ln->words = NULL;
            ln->numwords = 0;
        }
        ln->pos = linestartpos;
        ln->len = 0;
        ln->printwords = 0;
        for (wx2=0; wx2<ln->numwords; wx2++) {
            tbword_t *wd2 = &(ln->words[wx2]);
            ln->len += wd2->len;
            if (wd2->type != wd_EndLine && ln->len <= linewidth)
                ln->printwords = wx2+1;
        }
        linestartpos = lineeatpos;
        ln->startpara = (lastlinetype != wd_Text);
        lastlinetype = linetype;
        
        /* numwords, linestartpos, and startpara values are carried around
            to beginning of loop. */
    }
    
    return lx;
}

/* Replace lines[oldbeg, oldend) with tmplines[0, newnum). The replaced lines
    are deleted; the tmplines array winds up invalid (so it will not need to
    be deleted.) */
static void replace_lines(window_textbuffer_t *dwin, long oldbeg, long oldend,
    long newnum)
{
    long lx, diff;
    tbline_t *lines; /* cache */
    
    diff = newnum - (oldend - oldbeg);
    /* diff is the amount which lines will grow or shrink. */
    
    if (dwin->numlines+diff > dwin->linessize) {
        while (dwin->numlines+diff > dwin->linessize)
            dwin->linessize *= 2;
        dwin->lines = (tbline_t *)realloc(dwin->lines, 
            dwin->linessize * sizeof(tbline_t));
    }
    
    if (oldend > oldbeg)
        final_lines(dwin, oldbeg, oldend);
    
    lines = dwin->lines;

    if (diff != 0) {
        /* diff may be positive or negative */
        if (oldend < dwin->numlines)
            memmove(&(lines[oldend+diff]), &(lines[oldend]), 
                (dwin->numlines - oldend) * sizeof(tbline_t));
    }
    dwin->numlines += diff;
    
    if (newnum)
        memcpy(&(lines[oldbeg]), dwin->tmplines, newnum * sizeof(tbline_t));
        
    if (dwin->scrollline > oldend) {
        dwin->scrollline += diff;
    }
    else if (dwin->scrollline >= oldbeg) {
        dwin->scrollline = find_line_by_pos(dwin, dwin->scrollpos);
    }
    else {
        /* leave scrollline alone */
    }

    if (dwin->scrollline > dwin->numlines - dwin->height)
        dwin->scrollline = dwin->numlines - dwin->height;
    if (dwin->scrollline < 0)
        dwin->scrollline = 0;
}

static void updatetext(window_textbuffer_t *dwin)
{
    long drawbeg, drawend;
    
    if (dwin->dirtybeg != -1) {
        long numtmplines;
        long chbeg, chend; /* changed region */
        long oldchbeg, oldchend; /* old extent of changed region */
        long lnbeg, lnend; /* lines being replaced */
        long lndelta;
        int startpara;
        
        chbeg = dwin->dirtybeg;
        chend = dwin->dirtyend;
        oldchbeg = dwin->dirtybeg;
        oldchend = dwin->dirtyend - dwin->dirtydelta;
        
        /* push ahead to next newline or end-of-text (still in the same
            line as dirtyend, though). move chend and oldchend in parallel,
            since (outside the changed region) nothing has changed. */
        while (chend < dwin->numchars && dwin->chars[chend] != '\n') {
            chend++;
            oldchend++;
        }
        lnend = find_line_by_pos(dwin, oldchend) + 1;
        
        /* back up to beginning of line, or previous newline, whichever 
            is first */
        lnbeg = find_line_by_pos(dwin, oldchbeg); 
        if (lnbeg >= 0) {
            oldchbeg = dwin->lines[lnbeg].pos;
            chbeg = oldchbeg;
            startpara = dwin->lines[lnbeg].startpara;
        }
        else {
            lnbeg = 0;
            while (chbeg && dwin->chars[chbeg-1] != '\n') {
                chbeg--;
                oldchbeg--;
            }
            startpara = TRUE;
        }
        
        /* lnend is now the first line not to replace. [0..numlines]
            lnbeg is the first line *to* replace [0..numlines) */
        
        numtmplines = layout_chars(dwin, chbeg, chend, startpara);
        dwin->dirtybeg = -1;
        dwin->dirtyend = -1;
        dwin->dirtydelta = -1;
        
        replace_lines(dwin, lnbeg, lnend, numtmplines);
        lndelta = numtmplines - (lnend-lnbeg);
        
        drawbeg = lnbeg;
        if (lndelta == 0) {
            drawend = lnend;
        }
        else {
            if (lndelta > 0)
                drawend = dwin->numlines;
            else
                drawend = dwin->numlines - lndelta;
        }
    }
    else {
        drawbeg = 0;
        drawend = 0;
    }
    
    if (dwin->drawall) {
        drawbeg = dwin->scrollline;
        drawend = dwin->scrollline + dwin->height;
        dwin->drawall = FALSE;
    }
    
    if (drawend > drawbeg) {
        long lx, wx;
        int ix;
        int physln;
        int orgx, orgy;
        
        if (drawbeg < dwin->scrollline)
            drawbeg = dwin->scrollline;
        if (drawend > dwin->scrollline + dwin->height)
            drawend = dwin->scrollline + dwin->height;
        
        orgx = dwin->owner->bbox.left;
        orgy = dwin->owner->bbox.top;
        
        for (lx=drawbeg; lx<drawend; lx++) {
            physln = lx - dwin->scrollline;
            if (lx >= 0 && lx < dwin->numlines) {
                tbline_t *ln = &(dwin->lines[lx]);
                int count = 0;
                move(orgy+physln, orgx);
                for (wx=0; wx<ln->printwords; wx++) {
                    tbword_t *wd = &(ln->words[wx]);
                    if (wd->type == wd_Text || wd->type == wd_Blank) {
                        char *cx = &(dwin->chars[wd->pos]);
                        attrset(win_textbuffer_styleattrs[wd->style]);
                        for (ix=0; ix<wd->len; ix++, cx++, count++)
                            addch(*cx);
                    }
                }
                attrset(0);
                gli_print_spaces(dwin->width - count);
            }
            else {
                /* blank lines at bottom */
                move(orgy+physln, orgx);
                gli_print_spaces(dwin->width);
            }
        }
    }
}

void win_textbuffer_redraw(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    dwin->drawall = TRUE;
    updatetext(dwin);
}

void win_textbuffer_update(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    updatetext(dwin);
}

void win_textbuffer_putchar(window_t *win, char ch)
{
    window_textbuffer_t *dwin = win->data;
    long lx;
    
    if (dwin->numchars >= dwin->charssize) {
        dwin->charssize *= 2;
        dwin->chars = (char *)realloc(dwin->chars, 
            dwin->charssize * sizeof(char));
    }
    
    lx = dwin->numchars;
    
    if (win->style != dwin->runs[dwin->numruns-1].style) {
        set_last_run(dwin, win->style);
    }
    
    dwin->chars[lx] = ch;
    dwin->numchars++;
    
    if (dwin->dirtybeg == -1) {
        dwin->dirtybeg = lx;
        dwin->dirtyend = lx+1;
        dwin->dirtydelta = 1;
    }
    else {
        if (lx < dwin->dirtybeg)
            dwin->dirtybeg = lx;
        if (lx+1 > dwin->dirtyend)
            dwin->dirtyend = lx+1;
        dwin->dirtydelta += 1;
    }
}

static void set_last_run(window_textbuffer_t *dwin, glui32 style)
{
    long lx = dwin->numchars;
    long rx = dwin->numruns-1;
    
    if (dwin->runs[rx].pos == lx) {
        dwin->runs[rx].style = style;
    }
    else {
        rx++;
        if (rx >= dwin->runssize) {
            dwin->runssize *= 2;
            dwin->runs = (tbrun_t *)realloc(dwin->runs,
                dwin->runssize * sizeof(tbrun_t));
        }
        dwin->runs[rx].pos = lx;
        dwin->runs[rx].style = style;
        dwin->numruns++;
    }

}

/* This assumes that the text is all within the final style run. 
    Convenient, but true, since this is only used by editing in the
    input text. */
static void put_text(window_textbuffer_t *dwin, char *buf, long len, 
    long pos, long oldlen)
{
    long diff = len - oldlen;
    
    if (dwin->numchars + diff > dwin->charssize) {
        while (dwin->numchars + diff > dwin->charssize)
            dwin->charssize *= 2;
        dwin->chars = (char *)realloc(dwin->chars, 
            dwin->charssize * sizeof(char));
    }
    
    if (diff != 0 && pos+oldlen < dwin->numchars) {
        memmove(dwin->chars+(pos+len), dwin->chars+(pos+oldlen), 
            (dwin->numchars - (pos+oldlen) * sizeof(char)));
    }
    if (len > 0) {
        memmove(dwin->chars+pos, buf, len * sizeof(char));
    }
    dwin->numchars += diff;
    
    if (dwin->inbuf) {
        if (dwin->incurs >= pos+oldlen)
            dwin->incurs += diff;
        else if (dwin->incurs >= pos)
            dwin->incurs = pos+len;
    }
    
    if (dwin->dirtybeg == -1) {
        dwin->dirtybeg = pos;
        dwin->dirtyend = pos+len;
        dwin->dirtydelta = diff;
    }
    else {
        if (pos < dwin->dirtybeg)
            dwin->dirtybeg = pos;
        if (pos+len > dwin->dirtyend)
            dwin->dirtyend = pos+len;
        dwin->dirtydelta += diff;
    }
}

void win_textbuffer_clear(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    long oldlen = dwin->numchars;
    
    dwin->numchars = 0;
    dwin->numruns = 1;
    dwin->runs[0].style = win->style;
    dwin->runs[0].pos = 0;
    
    if (dwin->dirtybeg == -1) {
        dwin->dirtybeg = 0;
        dwin->dirtyend = 0;
        dwin->dirtydelta = -oldlen;
    }
    else {
        dwin->dirtybeg = 0;
        dwin->dirtyend = 0;
        dwin->dirtydelta -= oldlen;
    }

    dwin->scrollline = 0;
    dwin->scrollpos = 0;
    dwin->drawall = TRUE;
}

void win_textbuffer_place_cursor(window_t *win, int *xpos, int *ypos)
{
    window_textbuffer_t *dwin = win->data;
    int ix;

    if (win->line_request) {
        /* figure out where the input cursor is. */
        long lx = find_line_by_pos(dwin, dwin->incurs);
        if (lx < 0 || lx - dwin->scrollline < 0) {
            *ypos = 0;
            *xpos = 0;
        }
        else if (lx - dwin->scrollline >= dwin->height) {
            *ypos = dwin->height - 1;
            *xpos = dwin->width - 1;
        }
        else {
            *ypos = lx - dwin->scrollline;
            ix = dwin->incurs - dwin->lines[lx].pos;
            if (ix >= dwin->width)
                ix = dwin->width-1;
            *xpos = ix;
        }
    }
    else {
        /* put the cursor at the end of the text. */
        long lx = dwin->numlines - 1;
        if (lx < 0 || lx - dwin->scrollline < 0) {
            *ypos = 0;
            *xpos = 0;
        }
        else if (lx - dwin->scrollline >= dwin->height) {
            *ypos = dwin->height - 1;
            *xpos = dwin->width - 1;
        }
        else {
            *ypos = lx - dwin->scrollline;
            ix = dwin->lines[lx].len;
            if (ix >= dwin->width)
                ix = dwin->width-1;
            *xpos = ix;
        }
    }
}

/* Prepare the window for line input. */
void win_textbuffer_init_line(window_t *win, char *buf, int maxlen, 
    int initlen)
{
    window_textbuffer_t *dwin = win->data;
    
    dwin->inbuf = buf;
    dwin->inmax = maxlen;
    dwin->infence = dwin->numchars;
    dwin->incurs = dwin->numchars;
    dwin->origstyle = win->style;
    win->style = style_Input;
    set_last_run(dwin, win->style);
    
    if (initlen) {
        put_text(dwin, buf, initlen, dwin->incurs, 0);
    }
}

/* Abort line input, storing whatever's been typed so far. */
void win_textbuffer_cancel_line(window_t *win, event_t *ev)
{
    int ix;
    long len;
    window_textbuffer_t *dwin = win->data;

    if (!dwin->inbuf)
        return;
    
    len = dwin->numchars - dwin->infence;
    if (win->echostr) 
        gli_stream_echo_line(win->echostr, &(dwin->chars[dwin->infence]), len);

    if (len > dwin->inmax)
        len = dwin->inmax;
        
    for (ix=0; ix<len; ix++)
        dwin->inbuf[ix] = dwin->chars[dwin->infence+ix];
        
    win->style = dwin->origstyle;
    set_last_run(dwin, win->style);

    ev->type = evtype_LineInput;
    ev->win = WindowToID(win);
    ev->val1 = len;
    
    win->line_request = FALSE;
    dwin->inbuf = NULL;

    win_textbuffer_putchar(win, '\n');
}

/* Keybinding functions. */

/* Any key, during character input. Ends character input. */
void gcmd_buffer_accept_key(window_t *win, int arg)
{
    win->char_request = FALSE; 
    gli_event_store(evtype_CharInput, win, arg, 0);
}

/* Return or enter, during line input. Ends line input. */
void gcmd_buffer_accept_line(window_t *win, int arg)
{
    int ix;
    long len;
    window_textbuffer_t *dwin = win->data;
    
    if (!dwin->inbuf)
        return;
    
    len = dwin->numchars - dwin->infence;
    if (win->echostr) 
        gli_stream_echo_line(win->echostr, &(dwin->chars[dwin->infence]), len);
    
    if (len > dwin->inmax)
        len = dwin->inmax;
        
    for (ix=0; ix<len; ix++)
        dwin->inbuf[ix] = dwin->chars[dwin->infence+ix];
    
    win->style = dwin->origstyle;
    set_last_run(dwin, win->style);

    gli_event_store(evtype_LineInput, win, len, 0);
    win->line_request = FALSE;
    dwin->inbuf = NULL;
    
    win_textbuffer_putchar(win, '\n');
}

/* Any regular key, during line input. */
void gcmd_buffer_insert_key(window_t *win, int arg)
{
    window_textbuffer_t *dwin = win->data;
    char ch = arg;
    
    if (!dwin->inbuf)
        return;

    put_text(dwin, &ch, 1, dwin->incurs, 0);
    updatetext(dwin);
}

/* Cursor movement keys, during line input. */
void gcmd_buffer_move_cursor(window_t *win, int arg)
{
    window_textbuffer_t *dwin = win->data;
    
    if (!dwin->inbuf)
        return;

    switch (arg) {
        case gcmd_Left:
            if (dwin->incurs <= dwin->infence)
                return;
            dwin->incurs--;
            break;
        case gcmd_Right:
            if (dwin->incurs >= dwin->numchars)
                return;
            dwin->incurs++;
            break;
        case gcmd_LeftEnd:
            if (dwin->incurs <= dwin->infence)
                return;
            dwin->incurs = dwin->infence;
            break;
        case gcmd_RightEnd:
            if (dwin->incurs >= dwin->numchars)
                return;
            dwin->incurs = dwin->numchars;
            break;
    }
}

/* Delete keys, during line input. */
void gcmd_buffer_delete(window_t *win, int arg)
{
    window_textbuffer_t *dwin = win->data;
    
    if (!dwin->inbuf)
        return;

    switch (arg) {
        case gcmd_Delete:
            if (dwin->incurs <= dwin->infence)
                return;
            put_text(dwin, "", 0, dwin->incurs-1, 1);
            break;
        case gcmd_DeleteNext:
            if (dwin->incurs >= dwin->numchars)
                return;
            put_text(dwin, "", 0, dwin->incurs, 1);
            break;
        case gcmd_KillInput:
            if (dwin->infence >= dwin->numchars)
                return;
            put_text(dwin, "", 0, dwin->infence, 
                dwin->numchars - dwin->infence);
            break;
        case gcmd_KillLine:
            if (dwin->incurs >= dwin->numchars)
                return;
            put_text(dwin, "", 0, dwin->incurs, 
                dwin->numchars - dwin->incurs);
            break;
    }
    
    updatetext(dwin);
}

/* Scrolling keys, at all times. */
void gcmd_buffer_scroll(window_t *win, int arg)
{
    window_textbuffer_t *dwin = win->data;
    int maxval, minval, val;
    
    minval = 0;
    maxval = dwin->numlines - dwin->height;
    if (maxval < 0)
        maxval = 0;
    
    switch (arg) {
        case gcmd_UpEnd:
            val = minval;
            break;
        case gcmd_DownEnd:
            val = maxval;
            break;
        case gcmd_Up:
            val = dwin->scrollline - 1;
            break;
        case gcmd_Down:
            val = dwin->scrollline + 1;
            break;
        case gcmd_UpPage:
            val = dwin->scrollline - dwin->height;
            break;
        case gcmd_DownPage:
            val = dwin->scrollline + dwin->height;
            break;
    }
    
    if (val > maxval)
        val = maxval;
    if (val < minval)
        val = minval;
    
    if (val != dwin->scrollline) {
        dwin->scrollline = val;
        if (val >= dwin->numlines)
            dwin->scrollpos = dwin->numchars;
        else
            dwin->scrollpos = dwin->lines[val].pos;
        dwin->drawall = TRUE;
        updatetext(dwin);
    }
}
