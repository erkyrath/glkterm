/* gtwindow.c: Window objects
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef OPT_USE_SIGNALS
#include <signal.h>
#endif /* OPT_USE_SIGNALS */

#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_pair.h"
#include "gtw_blnk.h"
#include "gtw_grid.h"
#include "gtw_buf.h"

/* Linked list of all windows */
static window_t *gli_windowlist = NULL; 

/* For use by gli_print_spaces() */
#define NUMSPACES (16)
static char spacebuffer[NUMSPACES+1];

window_t *gli_rootwin = NULL; /* The topmost window. */
window_t *gli_focuswin = NULL; /* The window selected by the player. 
    (This has nothing to do with the "current output stream", which is
    gli_currentstr in gtstream.c. In fact, the program doesn't know
    about gli_focuswin at all.) */

/* This is the screen region which is enclosed by the root window. */
grect_t content_box;

void (*gli_interrupt_handler)(void) = NULL;

static void compute_content_box(void);

#ifdef OPT_USE_SIGNALS

    int just_resumed;
    int just_killed;
    static void gli_sig_resume(int val);
    static void gli_sig_interrupt(int val);

#ifdef OPT_WINCHANGED_SIGNAL
        int screen_size_changed;
        static void gli_sig_winsize(int val);
        static sig_t ncurses_sigwinch_handler = NULL;
#endif /* OPT_WINCHANGED_SIGNAL */

#endif /* OPT_USE_SIGNALS */

/* Set up the window system. This is called from main(). */
void gli_initialize_windows()
{
    int ix;
    
    gli_rootwin = NULL;
    gli_focuswin = NULL;
    
    /* Build a convenient array of spaces. */
    for (ix=0; ix<NUMSPACES; ix++)
        spacebuffer[ix] = ' ';
    spacebuffer[NUMSPACES] = '\0';
    
    /* Figure out the screen size. */
    compute_content_box();
    
#ifdef OPT_USE_SIGNALS

        just_resumed = FALSE;
        just_killed = FALSE;
        signal(SIGCONT, &gli_sig_resume);
        signal(SIGHUP, &gli_sig_interrupt);
        signal(SIGINT, &gli_sig_interrupt);

#ifdef OPT_WINCHANGED_SIGNAL
            screen_size_changed = FALSE;
            ncurses_sigwinch_handler = signal(SIGWINCH, &gli_sig_winsize);
#endif /* OPT_WINCHANGED_SIGNAL */

#endif /* OPT_USE_SIGNALS */

    /* Draw the initial setup (no windows) */
    gli_windows_redraw();
}

/* Set up all the curses parameters. This is called from main() -- 
    before gli_initialize_windows, actually -- and also when curses
    is reinitialized for a screen-size change. */
void gli_setup_curses()
{
    initscr();
    cbreak();
    noecho();
    nonl(); 
    intrflush(stdscr, FALSE); 
    keypad(stdscr, TRUE);
    scrollok(stdscr, FALSE);
}

#ifdef OPT_USE_SIGNALS

/* Signal handler for SIGCONT. */
static void gli_sig_resume(int val)
{
    signal(SIGCONT, &gli_sig_resume);
    just_resumed = TRUE;
}

/* Signal handler for SIGINT. */
static void gli_sig_interrupt(int val)
{
    just_killed = TRUE;
}

#ifdef OPT_WINCHANGED_SIGNAL

/* Signal handler for SIGWINCH. */
static void gli_sig_winsize(int val)
{
#ifdef KEY_RESIZE
    /* Ncurses can install a SIGWINCH handler that handles updates and repaints
        itself so we don't have to tear down and set up ncurses again (which
        clears the screen to reinitialize) and then pushes a KEY_RESIZE.
        However, ncurses can be built without the SIGWINCH handler, so we still
        need a fallback plan. */
    if (ncurses_sigwinch_handler && ncurses_sigwinch_handler != SIG_ERR) {
        ncurses_sigwinch_handler(val);
        signal(SIGWINCH, &gli_sig_winsize);
        return;
    }
#endif
    endwin();

    newterm(getenv("TERM"), stdout, stdin);
    gli_setup_curses();
    gcmd_win_resize(NULL, 0);
    signal(SIGWINCH, &gli_sig_winsize);
}

#endif /* OPT_WINCHANGED_SIGNAL */

#endif /* OPT_USE_SIGNALS */

/* Get out fast. This is used by the ctrl-C interrupt handler, under Unix. 
    It doesn't pause and wait for a keypress, and it calls the Glk interrupt
    handler. Otherwise it's the same as glk_exit(). */
void gli_fast_exit()
{
    if (gli_interrupt_handler) {
        (*gli_interrupt_handler)();
    }

    gli_streams_close_all();
    endwin();
    putchar('\n');
    exit(0);
}

static void compute_content_box()
{
    /* Set content_box to the entire screen, although one could also
        leave a border for messages or decoration. This is the only
        place where COLS and LINES are checked. All the rest of the
        layout code uses content_box. */
    int width, height;
    
    if (pref_screenwidth)
        width = pref_screenwidth;
    else
        width = COLS;
    if (pref_screenheight)
        height = pref_screenheight;
    else
        height = LINES;
    
    content_box.left = 0;
    content_box.top = 0;
    content_box.right = width;
    content_box.bottom = height;
    
    if (pref_messageline && height > 0)
        content_box.bottom--; /* allow a message line */
}

window_t *gli_new_window(glui32 type, glui32 rock)
{
    window_t *win = (window_t *)malloc(sizeof(window_t));
    if (!win)
        return NULL;
    
    win->magicnum = MAGIC_WINDOW_NUM;
    win->rock = rock;
    win->type = type;
    
    win->parent = NULL; /* for now */
    win->data = NULL; /* for now */
    win->char_request = FALSE;
    win->line_request = FALSE;
    win->line_request_uni = FALSE;
    win->char_request_uni = FALSE;
    win->echo_line_input = TRUE;
    win->terminate_line_input = 0;
    gli_initialize_window_styles(win);

    win->str = gli_stream_open_window(win);
    win->echostr = NULL;

    win->prev = NULL;
    win->next = gli_windowlist;
    gli_windowlist = win;
    if (win->next) {
        win->next->prev = win;
    }
    
    if (gli_register_obj)
        win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);
    
    return win;
}

void gli_delete_window(window_t *win)
{
    window_t *prev, *next;
    
    if (gli_unregister_obj)
        (*gli_unregister_obj)(win, gidisp_Class_Window, win->disprock);
        
    win->magicnum = 0;
    
    win->echostr = NULL;
    if (win->str) {
        gli_delete_stream(win->str);
        win->str = NULL;
    }
    gli_destroy_window_styles(win);
    
    prev = win->prev;
    next = win->next;
    win->prev = NULL;
    win->next = NULL;

    if (prev)
        prev->next = next;
    else
        gli_windowlist = next;
    if (next)
        next->prev = prev;
        
    free(win);
}

winid_t glk_window_open(winid_t splitwin, glui32 method, glui32 size, 
    glui32 wintype, glui32 rock)
{
    window_t *newwin, *pairwin, *oldparent;
    window_pair_t *dpairwin;
    grect_t box;
    glui32 val;
    
    if (!gli_rootwin) {
        if (splitwin) {
            gli_strict_warning("window_open: ref must be NULL");
            return 0;
        }
        /* ignore method and size now */
        oldparent = NULL;
        
        box = content_box;
    }
    else {
    
        if (!splitwin) {
            gli_strict_warning("window_open: ref must not be NULL");
            return 0;
        }
        
        val = (method & winmethod_DivisionMask);
        if (val != winmethod_Fixed && val != winmethod_Proportional) {
            gli_strict_warning("window_open: invalid method (not fixed or proportional)");
            return 0;
        }
        
        val = (method & winmethod_DirMask);
        if (val != winmethod_Above && val != winmethod_Below 
            && val != winmethod_Left && val != winmethod_Right) {
            gli_strict_warning("window_open: invalid method (bad direction)");
            return 0;
        }

        box = splitwin->bbox;
        
        oldparent = splitwin->parent;
        if (oldparent && oldparent->type != wintype_Pair) {
            gli_strict_warning("window_open: parent window is not Pair");
            return 0;
        }
    
    }
    
    newwin = gli_new_window(wintype, rock);
    if (!newwin) {
        gli_strict_warning("window_open: unable to create window");
        return 0;
    }
    
    switch (wintype) {
        case wintype_Blank:
            newwin->data = win_blank_create(newwin);
            break;
        case wintype_TextGrid:
            newwin->data = win_textgrid_create(newwin);
            break;
        case wintype_TextBuffer:
            newwin->data = win_textbuffer_create(newwin);
            break;
        case wintype_Pair:
            gli_strict_warning("window_open: cannot open pair window directly");
            gli_delete_window(newwin);
            return 0;
        default:
            /* Unknown window type -- do not print a warning, just return 0
                to indicate that it's not possible. */
            gli_delete_window(newwin);
            return 0;
    }
    
    if (!newwin->data) {
        gli_strict_warning("window_open: unable to create window");
        return 0;
    }
    
    if (!splitwin) {
        gli_rootwin = newwin;
        gli_window_rearrange(newwin, &box);
        /* redraw everything, which is just the new first window */
        gli_windows_redraw();
    }
    else {
        /* create pairwin, with newwin as the key */
        pairwin = gli_new_window(wintype_Pair, 0);
        dpairwin = win_pair_create(pairwin, method, newwin, size);
        pairwin->data = dpairwin;
            
        dpairwin->child1 = splitwin;
        dpairwin->child2 = newwin;
        
        splitwin->parent = pairwin;
        newwin->parent = pairwin;
        pairwin->parent = oldparent;

        if (oldparent) {
            window_pair_t *dparentwin = oldparent->data;
            if (dparentwin->child1 == splitwin)
                dparentwin->child1 = pairwin;
            else
                dparentwin->child2 = pairwin;
        }
        else {
            gli_rootwin = pairwin;
        }
        
        gli_window_rearrange(pairwin, &box);
        /* redraw the new pairwin and all its contents */
        gli_window_redraw(pairwin);
    }
    
    return newwin;
}

static void gli_window_close(window_t *win, int recurse)
{
    window_t *wx;
    
    if (gli_focuswin == win) {
        gli_focuswin = NULL;
    }
    
    for (wx=win->parent; wx; wx=wx->parent) {
        if (wx->type == wintype_Pair) {
            window_pair_t *dwx = wx->data;
            if (dwx->key == win) {
                dwx->key = NULL;
                dwx->keydamage = TRUE;
            }
        }
    }
    
    switch (win->type) {
        case wintype_Blank: {
            window_blank_t *dwin = win->data;
            win_blank_destroy(dwin);
            }
            break;
        case wintype_Pair: {
            window_pair_t *dwin = win->data;
            if (recurse) {
                if (dwin->child1)
                    gli_window_close(dwin->child1, TRUE);
                if (dwin->child2)
                    gli_window_close(dwin->child2, TRUE);
            }
            win_pair_destroy(dwin);
            }
            break;
        case wintype_TextBuffer: {
            window_textbuffer_t *dwin = win->data;
            win_textbuffer_destroy(dwin);
            }
            break;
        case wintype_TextGrid: {
            window_textgrid_t *dwin = win->data;
            win_textgrid_destroy(dwin);
            }
            break;
    }
    
    gli_delete_window(win);
}

void glk_window_close(window_t *win, stream_result_t *result)
{
    if (!win) {
        gli_strict_warning("window_close: invalid ref");
        return;
    }
        
    if (win == gli_rootwin || win->parent == NULL) {
        /* close the root window, which means all windows. */
        
        gli_rootwin = 0;
        
        /* begin (simpler) closation */
        
        gli_stream_fill_result(win->str, result);
        gli_window_close(win, TRUE); 
        /* redraw everything */
        gli_windows_redraw();
    }
    else {
        /* have to jigger parent */
        grect_t box;
        window_t *pairwin, *sibwin, *grandparwin, *wx;
        window_pair_t *dpairwin, *dgrandparwin, *dwx;
        int keydamage_flag;
        
        pairwin = win->parent;
        dpairwin = pairwin->data;
        if (win == dpairwin->child1) {
            sibwin = dpairwin->child2;
        }
        else if (win == dpairwin->child2) {
            sibwin = dpairwin->child1;
        }
        else {
            gli_strict_warning("window_close: window tree is corrupted");
            return;
        }
        
        box = pairwin->bbox;

        grandparwin = pairwin->parent;
        if (!grandparwin) {
            gli_rootwin = sibwin;
            sibwin->parent = NULL;
        }
        else {
            dgrandparwin = grandparwin->data;
            if (dgrandparwin->child1 == pairwin)
                dgrandparwin->child1 = sibwin;
            else
                dgrandparwin->child2 = sibwin;
            sibwin->parent = grandparwin;
        }
        
        /* Begin closation */
        
        gli_stream_fill_result(win->str, result);

        /* Close the child window (and descendants), so that key-deletion can
            crawl up the tree to the root window. */
        gli_window_close(win, TRUE); 
        
        /* This probably isn't necessary, but the child *is* gone, so just
            in case. */
        if (win == dpairwin->child1) {
            dpairwin->child1 = NULL;
        }
        else if (win == dpairwin->child2) {
            dpairwin->child2 = NULL;
        }
        
        /* Now we can delete the parent pair. */
        gli_window_close(pairwin, FALSE);

        keydamage_flag = FALSE;
        for (wx=sibwin; wx; wx=wx->parent) {
            if (wx->type == wintype_Pair) {
                window_pair_t *dwx = wx->data;
                if (dwx->keydamage) {
                    keydamage_flag = TRUE;
                    dwx->keydamage = FALSE;
                }
            }
        }
        
        if (keydamage_flag) {
            box = content_box;
            gli_window_rearrange(gli_rootwin, &box);
            gli_windows_redraw();
        }
        else {
            gli_window_rearrange(sibwin, &box);
            gli_window_redraw(sibwin);
        }
    }
}

void glk_window_get_arrangement(window_t *win, glui32 *method, glui32 *size, 
    winid_t *keywin)
{
    window_pair_t *dwin;
    glui32 val;
    
    if (!win) {
        gli_strict_warning("window_get_arrangement: invalid ref");
        return;
    }
    
    if (win->type != wintype_Pair) {
        gli_strict_warning("window_get_arrangement: not a Pair window");
        return;
    }
    
    dwin = win->data;
    
    val = dwin->dir | dwin->division;
    if (!dwin->hasborder)
        val |= winmethod_NoBorder;
    
    if (size)
        *size = dwin->size;
    if (keywin) {
        if (dwin->key)
            *keywin = dwin->key;
        else
            *keywin = NULL;
    }
    if (method)
        *method = val;
}

void glk_window_set_arrangement(window_t *win, glui32 method, glui32 size, 
    winid_t key)
{
    window_pair_t *dwin;
    glui32 newdir;
    grect_t box;
    int newvertical, newbackward;
    
    if (!win) {
        gli_strict_warning("window_set_arrangement: invalid ref");
        return;
    }
    
    if (win->type != wintype_Pair) {
        gli_strict_warning("window_set_arrangement: not a Pair window");
        return;
    }
    
    if (key) {
        window_t *wx;
        if (key->type == wintype_Pair) {
            gli_strict_warning("window_set_arrangement: keywin cannot be a Pair");
            return;
        }
        for (wx=key; wx; wx=wx->parent) {
            if (wx == win)
                break;
        }
        if (wx == NULL) {
            gli_strict_warning("window_set_arrangement: keywin must be a descendant");
            return;
        }
    }
    
    dwin = win->data;
    box = win->bbox;
    
    newdir = method & winmethod_DirMask;
    newvertical = (newdir == winmethod_Left || newdir == winmethod_Right);
    newbackward = (newdir == winmethod_Left || newdir == winmethod_Above);
    if (!key)
        key = dwin->key;

    if ((newvertical && !dwin->vertical) || (!newvertical && dwin->vertical)) {
        if (!dwin->vertical)
            gli_strict_warning("window_set_arrangement: split must stay horizontal");
        else
            gli_strict_warning("window_set_arrangement: split must stay vertical");
        return;
    }
    
    if (key && key->type == wintype_Blank 
        && (method & winmethod_DivisionMask) == winmethod_Fixed) {
        gli_strict_warning("window_set_arrangement: a Blank window cannot have a fixed size");
        return;
    }

    if ((newbackward && !dwin->backward) || (!newbackward && dwin->backward)) {
        /* switch the children */
        window_t *tmpwin = dwin->child1;
        dwin->child1 = dwin->child2;
        dwin->child2 = tmpwin;
    }
    
    /* set up everything else */
    dwin->dir = newdir;
    dwin->division = method & winmethod_DivisionMask;
    dwin->key = key;
    dwin->size = size;
    dwin->hasborder = ((method & winmethod_BorderMask) == winmethod_Border);
    
    dwin->vertical = (dwin->dir == winmethod_Left || dwin->dir == winmethod_Right);
    dwin->backward = (dwin->dir == winmethod_Left || dwin->dir == winmethod_Above);
    
    gli_window_rearrange(win, &box);
    gli_window_redraw(win);
}

winid_t glk_window_iterate(winid_t win, glui32 *rock)
{
    if (!win) {
        win = gli_windowlist;
    }
    else {
        win = win->next;
    }
    
    if (win) {
        if (rock)
            *rock = win->rock;
        return win;
    }
    
    if (rock)
        *rock = 0;
    return NULL;
}

window_t *gli_window_iterate_treeorder(window_t *win)
{
    if (!win)
        return gli_rootwin;
    
    if (win->type == wintype_Pair) {
        window_pair_t *dwin = win->data;
        if (!dwin->backward)
            return dwin->child1;
        else
            return dwin->child2;
    }
    else {
        window_t *parwin;
        window_pair_t *dwin;
        
        while (win->parent) {
            parwin = win->parent;
            dwin = parwin->data;
            if (!dwin->backward) {
                if (win == dwin->child1)
                    return dwin->child2;
            }
            else {
                if (win == dwin->child2)
                    return dwin->child1;
            }
            win = parwin;
        }
        
        return NULL;
    }
}

glui32 glk_window_get_rock(window_t *win)
{
    if (!win) {
        gli_strict_warning("window_get_rock: invalid ref.");
        return 0;
    }
    
    return win->rock;
}

winid_t glk_window_get_root()
{
    if (!gli_rootwin)
        return NULL;
    return gli_rootwin;
}

winid_t glk_window_get_parent(window_t *win)
{
    if (!win) {
        gli_strict_warning("window_get_parent: invalid ref");
        return 0;
    }
    if (win->parent)
        return win->parent;
    else
        return 0;
}

winid_t glk_window_get_sibling(window_t *win)
{
    window_pair_t *dparwin;
    
    if (!win) {
        gli_strict_warning("window_get_sibling: invalid ref");
        return 0;
    }
    if (!win->parent)
        return 0;
    
    dparwin = win->parent->data;
    if (dparwin->child1 == win)
        return dparwin->child2;
    else if (dparwin->child2 == win)
        return dparwin->child1;
    return 0;
}

glui32 glk_window_get_type(window_t *win)
{
    if (!win) {
        gli_strict_warning("window_get_parent: invalid ref");
        return 0;
    }
    return win->type;
}

void glk_window_get_size(window_t *win, glui32 *width, glui32 *height)
{
    glui32 wid = 0;
    glui32 hgt = 0;
    
    if (!win) {
        gli_strict_warning("window_get_size: invalid ref");
        return;
    }
    
    switch (win->type) {
        case wintype_Blank:
        case wintype_Pair:
            /* always zero */
            break;
        case wintype_TextGrid:
        case wintype_TextBuffer:
            wid = win->bbox.right - win->bbox.left;
            hgt = win->bbox.bottom - win->bbox.top;
            break;
    }

    if (width)
        *width = wid;
    if (height)
        *height = hgt;
}

strid_t glk_window_get_stream(window_t *win)
{
    if (!win) {
        gli_strict_warning("window_get_stream: invalid ref");
        return NULL;
    }
    
    return win->str;
}

strid_t glk_window_get_echo_stream(window_t *win)
{
    if (!win) {
        gli_strict_warning("window_get_echo_stream: invalid ref");
        return 0;
    }
    
    if (win->echostr)
        return win->echostr;
    else
        return 0;
}

void glk_window_set_echo_stream(window_t *win, stream_t *str)
{
    if (!win) {
        gli_strict_warning("window_set_echo_stream: invalid window id");
        return;
    }
    
    win->echostr = str;
}

void glk_set_window(window_t *win)
{
    if (!win) {
        gli_stream_set_current(NULL);
    }
    else {
        gli_stream_set_current(win->str);
    }
}

void gli_windows_unechostream(stream_t *str)
{
    window_t *win;
    
    for (win=gli_windowlist; win; win=win->next) {
        if (win->echostr == str)
            win->echostr = NULL;
    }
}

/* Some trivial switch functions which make up for the fact that we're not
    doing this in C++. */

void gli_window_rearrange(window_t *win, grect_t *box)
{
    switch (win->type) {
        case wintype_Blank:
            win_blank_rearrange(win, box);
            break;
        case wintype_Pair:
            win_pair_rearrange(win, box);
            break;
        case wintype_TextGrid:
            win_textgrid_rearrange(win, box);
            break;
        case wintype_TextBuffer:
            win_textbuffer_rearrange(win, box);
            break;
    }
}

void gli_windows_update()
{
    window_t *win;
    
    for (win=gli_windowlist; win; win=win->next) {
        switch (win->type) {
            case wintype_TextGrid:
                win_textgrid_update(win);
                break;
            case wintype_TextBuffer:
                win_textbuffer_update(win);
                break;
        }
    }
}

void gli_window_redraw(window_t *win)
{
    if (win->bbox.left >= win->bbox.right 
        || win->bbox.top >= win->bbox.bottom)
        return;
    
    switch (win->type) {
        case wintype_Blank:
            win_blank_redraw(win);
            break;
        case wintype_Pair:
            win_pair_redraw(win);
            break;
        case wintype_TextGrid:
            win_textgrid_redraw(win);
            break;
        case wintype_TextBuffer:
            win_textbuffer_redraw(win);
            break;
    }
}

void gli_windows_redraw()
{
    int ix, jx;
    
    if (gli_rootwin) {
        /* We could draw a border around content_box, if we wanted. */
        gli_window_redraw(gli_rootwin);
    }
    else {
        /* There are no windows at all. */
        clear();
        ix = (content_box.left+content_box.right) / 2 - 7;
        if (ix < 0)
            ix = 0;
        jx = (content_box.top+content_box.bottom) / 2;
        move(jx, ix);
        addstr("Please wait...");
    }
}

void gli_windows_size_change()
{
    compute_content_box();
    if (gli_rootwin) {
        gli_window_rearrange(gli_rootwin, &content_box);
    }
    gli_windows_redraw();
    gli_msgline_redraw();
    
    gli_event_store(evtype_Arrange, NULL, 0, 0);
}

void gli_windows_place_cursor()
{
    if (gli_rootwin && gli_focuswin) {
        int xpos, ypos;
        xpos = 0;
        ypos = 0;
        switch (gli_focuswin->type) {
            case wintype_TextGrid: 
                win_textgrid_place_cursor(gli_focuswin, &xpos, &ypos);
                break;
            case wintype_TextBuffer: 
                win_textbuffer_place_cursor(gli_focuswin, &xpos, &ypos);
                break;
            default:
                break;
        }
        move(gli_focuswin->bbox.top + ypos, gli_focuswin->bbox.left + xpos);
    }
    else {
        move(content_box.bottom-1, content_box.right-1);
    }
}

void gli_windows_set_paging(int forcetoend)
{
    window_t *win;
    
    for (win=gli_windowlist; win; win=win->next) {
        switch (win->type) {
            case wintype_TextBuffer:
                win_textbuffer_set_paging(win, forcetoend);
                break;
        }
    }
}

void gli_windows_trim_buffers()
{
    window_t *win;
    
    for (win=gli_windowlist; win; win=win->next) {
        switch (win->type) {
            case wintype_TextBuffer:
                win_textbuffer_trim_buffer(win);
                break;
        }
    }
}

void glk_request_char_event(window_t *win)
{
    if (!win) {
        gli_strict_warning("request_char_event: invalid ref");
        return;
    }
    
    if (win->char_request || win->line_request) {
        gli_strict_warning("request_char_event: window already has keyboard request");
        return;
    }
    
    switch (win->type) {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = TRUE;
            win->char_request_uni = FALSE;
            break;
        default:
            gli_strict_warning("request_char_event: window does not support keyboard input");
            break;
    }
    
}

void glk_request_line_event(window_t *win, char *buf, glui32 maxlen, 
    glui32 initlen)
{
    if (!win) {
        gli_strict_warning("request_line_event: invalid ref");
        return;
    }
    
    if (win->char_request || win->line_request) {
        gli_strict_warning("request_line_event: window already has keyboard request");
        return;
    }
    
    switch (win->type) {
        case wintype_TextBuffer:
            win->line_request = TRUE;
            win->line_request_uni = FALSE;
            win_textbuffer_init_line(win, buf, FALSE, maxlen, initlen);
            break;
        case wintype_TextGrid:
            win->line_request = TRUE;
            win->line_request_uni = FALSE;
            win_textgrid_init_line(win, buf, FALSE, maxlen, initlen);
            break;
        default:
            gli_strict_warning("request_line_event: window does not support keyboard input");
            break;
    }
    
}

#ifdef GLK_MODULE_UNICODE

void glk_request_char_event_uni(window_t *win)
{
    if (!win) {
        gli_strict_warning("request_char_event: invalid ref");
        return;
    }
    
    if (win->char_request || win->line_request) {
        gli_strict_warning("request_char_event: window already has keyboard request");
        return;
    }
    
    switch (win->type) {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = TRUE;
            win->char_request_uni = TRUE;
            break;
        default:
            gli_strict_warning("request_char_event: window does not support keyboard input");
            break;
    }
    
}

void glk_request_line_event_uni(window_t *win, glui32 *buf, glui32 maxlen, 
    glui32 initlen)
{
    if (!win) {
        gli_strict_warning("request_line_event: invalid ref");
        return;
    }
    
    if (win->char_request || win->line_request) {
        gli_strict_warning("request_line_event: window already has keyboard request");
        return;
    }
    
    switch (win->type) {
        case wintype_TextBuffer:
            win->line_request = TRUE;
            win->line_request_uni = TRUE;
            win_textbuffer_init_line(win, buf, TRUE, maxlen, initlen);
            break;
        case wintype_TextGrid:
            win->line_request = TRUE;
            win->line_request_uni = TRUE;
            win_textgrid_init_line(win, buf, TRUE, maxlen, initlen);
            break;
        default:
            gli_strict_warning("request_line_event: window does not support keyboard input");
            break;
    }
    
}

#endif /* GLK_MODULE_UNICODE */

void glk_request_mouse_event(window_t *win)
{
    if (!win) {
        gli_strict_warning("request_mouse_event: invalid ref");
        return;
    }
    
    /* But, in fact, we can't do much about this. */
    
    return;
}

void glk_cancel_char_event(window_t *win)
{
    if (!win) {
        gli_strict_warning("cancel_char_event: invalid ref");
        return;
    }
    
    switch (win->type) {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = FALSE;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_line_event(window_t *win, event_t *ev)
{
    event_t dummyev;
    
    if (!ev) {
        ev = &dummyev;
    }

    gli_event_clearevent(ev);
    
    if (!win) {
        gli_strict_warning("cancel_line_event: invalid ref");
        return;
    }
    
    switch (win->type) {
        case wintype_TextBuffer:
            if (win->line_request) {
                win_textbuffer_cancel_line(win, ev);
            }
            break;
        case wintype_TextGrid:
            if (win->line_request) {
                win_textgrid_cancel_line(win, ev);
            }
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_mouse_event(window_t *win)
{
    if (!win) {
        gli_strict_warning("cancel_mouse_event: invalid ref");
        return;
    }
    
    /* But, in fact, we can't do much about this. */
    
    return;
}

void gli_window_put_char(window_t *win, char ch)
{
    /* Character set conversion is necessary here, since we're printing to
        native (curses.h) output routines. */
    
    if (!char_printable_table[(unsigned char)ch]) {
        char *altstr = gli_ascii_equivalent(ch);
        /* altstr is now a sensible ASCII equivalent, or else an octal
            code like "\177". Call gli_window_put_char() recursively to
            print it. This is safe, if funky, because altstr contains
            only characters in the range 0x20..0x7E. */
        while (*altstr) {
            gli_window_put_char(win, *altstr);
            altstr++;
        }
        return;
    }
    
#ifndef OPT_NATIVE_LATIN_1  
    ch = char_to_native_table[(unsigned char)ch];
#endif /* OPT_NATIVE_LATIN_1 */

    switch (win->type) {
        case wintype_TextBuffer:
            win_textbuffer_putchar(win, ch);
            break;
        case wintype_TextGrid:
            win_textgrid_putchar(win, ch);
            break;
    }
}

void glk_window_clear(window_t *win)
{
    if (!win) {
        gli_strict_warning("window_clear: invalid ref");
        return;
    }
    
    if (win->line_request) {
        gli_strict_warning("window_clear: window has pending line request");
        return;
    }

    switch (win->type) {
        case wintype_TextBuffer:
            win_textbuffer_clear(win);
            break;
        case wintype_TextGrid:
            win_textgrid_clear(win);
            break;
    }
}

void glk_window_move_cursor(window_t *win, glui32 xpos, glui32 ypos)
{
    if (!win) {
        gli_strict_warning("window_move_cursor: invalid ref");
        return;
    }
    
    switch (win->type) {
        case wintype_TextGrid:
            win_textgrid_move_cursor(win, xpos, ypos);
            break;
        default:
            gli_strict_warning("window_move_cursor: not a TextGrid window");
            break;
    }
}

void gli_print_spaces(int len)
{
    while (len >= NUMSPACES) {
        addstr(spacebuffer);
        len -= NUMSPACES;
    }
    
    if (len > 0) {
        addstr(&(spacebuffer[NUMSPACES - len]));
    }
}

#ifdef GLK_MODULE_LINE_ECHO

void glk_set_echo_line_event(window_t *win, glui32 val)
{
    if (!win) {
        gli_strict_warning("set_echo_line_event: invalid ref");
        return;
    }
    
    win->echo_line_input = (val != 0);
}

#endif /* GLK_MODULE_LINE_ECHO */

#ifdef GLK_MODULE_LINE_TERMINATORS

void glk_set_terminators_line_event(window_t *win, glui32 *keycodes, 
    glui32 count)
{
    int ix;
    glui32 res, val;

    if (!win) {
        gli_strict_warning("set_terminators_line_event: invalid ref");
        return;
    }
    
    /* We only allow escape and the function keys as line input terminators.
       We encode those in a bitmask. */
    res = 0;
    if (keycodes) {
        for (ix=0; ix<count; ix++) {
            if (keycodes[ix] == keycode_Escape) {
                res |= 0x10000;
            }
            else {
                val = keycode_Func1 + 1 - keycodes[ix];
                if (val >= 1 && val <= 12)
                    res |= (1 << val);
            }
        }
    }

    win->terminate_line_input = res;
}

#endif /* GLK_MODULE_LINE_TERMINATORS */

/* Keybinding functions. */

void gcmd_win_change_focus(window_t *win, glui32 arg)
{
    win = gli_window_iterate_treeorder(gli_focuswin);
    while (win == NULL || win->type == wintype_Pair) {
        if (win == gli_focuswin)
            return;
        win = gli_window_iterate_treeorder(win);
    }
    
    gli_focuswin = win;
}

void gcmd_win_refresh(window_t *win, glui32 arg)
{
    clear();
    gli_windows_redraw();
    gli_msgline_redraw();
    wrefresh(curscr);
}

void gcmd_win_resize(window_t *win, glui32 arg)
{
    gli_set_halfdelay();
    screen_size_changed = TRUE;
}

#ifdef GLK_MODULE_IMAGE

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
    gli_strict_warning("image_draw: graphics not supported.");
    return FALSE;
}

glui32 glk_image_draw_scaled(winid_t win, glui32 image, 
    glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
    gli_strict_warning("image_draw_scaled: graphics not supported.");
    return FALSE;
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
    gli_strict_warning("image_get_info: graphics not supported.");
    return FALSE;
}

void glk_window_flow_break(winid_t win)
{
    gli_strict_warning("window_flow_break: graphics not supported.");
}

void glk_window_erase_rect(winid_t win, 
    glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    gli_strict_warning("window_erase_rect: graphics not supported.");
}

void glk_window_fill_rect(winid_t win, glui32 color, 
    glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    gli_strict_warning("window_fill_rect: graphics not supported.");
}

void glk_window_set_background_color(winid_t win, glui32 color)
{
    gli_strict_warning("window_set_background_color: graphics not supported.");
}

#endif /* GLK_MODULE_IMAGE */

#ifdef GLK_MODULE_HYPERLINKS

void glk_set_hyperlink(glui32 linkval)
{
    gli_strict_warning("set_hyperlink: hyperlinks not supported.");
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
    gli_strict_warning("set_hyperlink_stream: hyperlinks not supported.");
}

void glk_request_hyperlink_event(winid_t win)
{
    gli_strict_warning("request_hyperlink_event: hyperlinks not supported.");
}

void glk_cancel_hyperlink_event(winid_t win)
{
    gli_strict_warning("cancel_hyperlink_event: hyperlinks not supported.");
}

#endif /* GLK_MODULE_HYPERLINKS */
