/* gtinput.c: Key input handling
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_grid.h"
#include "gtw_buf.h"

typedef void (*command_fptr)(window_t *win, glui32);

typedef struct command_struct {
    command_fptr func;
    int arg;
} command_t;

/* The idea is that, depending on what kind of window has focus and
    whether it is set for line or character input, various keys are
    bound to various command_t objects. A command_t contains a function
    to call to handle the key, and an argument. (This allows one
    function to handle several variants of a command -- for example,
    gcmd_buffer_scroll() handles scrolling both up and down.) If the
    argument is -1, the function will be passed the actual key hit.
    (This allows a single function to handle a range of keys.) 
   Key values may be 0 to 255, or any of the special KEY_* values
    defined in curses.h. */

/* Keys which are always meaningful. */
static command_t *commands_always(int key)
{
    static command_t cmdchangefocus = { gcmd_win_change_focus, 0 };
    static command_t cmdrefresh = { gcmd_win_refresh, 0 };

    switch (key) {
        case '\t': 
            return &cmdchangefocus;
        case '\014': /* ctrl-L */
            return &cmdrefresh;
    }
    
    return NULL;
}

/* Keys which are always meaningful in a text grid window. */
static command_t *commands_textgrid(int key)
{
    return NULL;
}

/* Keys for char input in a text grid window. */
static command_t *commands_textgrid_char(int key)
{
    static command_t cmdv = { gcmd_grid_accept_key, -1 };

    return &cmdv;
}

/* Keys for line input in a text grid window. */
static command_t *commands_textgrid_line(window_textgrid_t *dwin, int key)
{
    static command_t cmdacceptline = { gcmd_grid_accept_line, 0 };
    static command_t cmdacceptlineterm = { gcmd_grid_accept_line, -1 };
    static command_t cmdinsert = { gcmd_grid_insert_key, -1 };
    static command_t cmdmoveleft = { gcmd_grid_move_cursor, gcmd_Left };
    static command_t cmdmoveright = { gcmd_grid_move_cursor, gcmd_Right };
    static command_t cmdmoveleftend = { gcmd_grid_move_cursor, gcmd_LeftEnd };
    static command_t cmdmoverightend = { gcmd_grid_move_cursor, gcmd_RightEnd };
    static command_t cmddelete = { gcmd_grid_delete, gcmd_Delete };
    static command_t cmddeletenext = { gcmd_grid_delete, gcmd_DeleteNext };
    static command_t cmdkillinput = { gcmd_grid_delete, gcmd_KillInput };
    static command_t cmdkillline = { gcmd_grid_delete, gcmd_KillLine };

    if (key >= 32 && key < 256 && key != 127) 
        return &cmdinsert;
    switch (key) {
        case KEY_ENTER:
        case '\012': /* ctrl-J */
        case '\015': /* ctrl-M */
            return &cmdacceptline;
        case KEY_LEFT:
        case '\002': /* ctrl-B */
            return &cmdmoveleft;
        case KEY_RIGHT:
        case '\006': /* ctrl-F */
            return &cmdmoveright;
        case KEY_HOME:
        case '\001': /* ctrl-A */
            return &cmdmoveleftend;
        case KEY_END:
        case '\005': /* ctrl-E */
            return &cmdmoverightend;
        case '\177': /* delete */
        case '\010': /* backspace */
        case KEY_BACKSPACE:
        case KEY_DC:
            return &cmddelete;
        case '\004': /* ctrl-D */
            return &cmddeletenext;
        case '\013': /* ctrl-K */
            return &cmdkillline;
        case '\025': /* ctrl-U */
            return &cmdkillinput;

        case '\033': /* escape */
            if (dwin->intermkeys & 0x10000)
                return &cmdacceptlineterm;
            break;
#ifdef KEY_F
        case KEY_F(1):
            if (dwin->intermkeys & (1<<1))
                return &cmdacceptlineterm;
            break;
        case KEY_F(2):
            if (dwin->intermkeys & (1<<2))
                return &cmdacceptlineterm;
            break;
        case KEY_F(3):
            if (dwin->intermkeys & (1<<3))
                return &cmdacceptlineterm;
            break;
        case KEY_F(4):
            if (dwin->intermkeys & (1<<4))
                return &cmdacceptlineterm;
            break;
        case KEY_F(5):
            if (dwin->intermkeys & (1<<5))
                return &cmdacceptlineterm;
            break;
        case KEY_F(6):
            if (dwin->intermkeys & (1<<6))
                return &cmdacceptlineterm;
            break;
        case KEY_F(7):
            if (dwin->intermkeys & (1<<7))
                return &cmdacceptlineterm;
            break;
        case KEY_F(8):
            if (dwin->intermkeys & (1<<8))
                return &cmdacceptlineterm;
            break;
        case KEY_F(9):
            if (dwin->intermkeys & (1<<9))
                return &cmdacceptlineterm;
            break;
        case KEY_F(10):
            if (dwin->intermkeys & (1<<10))
                return &cmdacceptlineterm;
            break;
        case KEY_F(11):
            if (dwin->intermkeys & (1<<11))
                return &cmdacceptlineterm;
            break;
        case KEY_F(12):
            if (dwin->intermkeys & (1<<12))
                return &cmdacceptlineterm;
            break;
#endif /* KEY_F */
    }
    return NULL;
}

/* Keys which are always meaningful in a text buffer window. Note that
    these override character input, which means you can never type ctrl-Y
    or ctrl-V in a textbuffer, even though you can in a textgrid. The Glk
    API doesn't make this distinction. Damn. */
static command_t *commands_textbuffer(int key)
{
    static command_t cmdscrolltotop = { gcmd_buffer_scroll, gcmd_UpEnd };
    static command_t cmdscrolltobottom = { gcmd_buffer_scroll, gcmd_DownEnd };
    static command_t cmdscrollupline = { gcmd_buffer_scroll, gcmd_Up };
    static command_t cmdscrolldownline = { gcmd_buffer_scroll, gcmd_Down };
    static command_t cmdscrolluppage = { gcmd_buffer_scroll, gcmd_UpPage };
    static command_t cmdscrolldownpage = { gcmd_buffer_scroll, gcmd_DownPage };

    switch (key) {
        case KEY_HOME:
            return &cmdscrolltotop;
        case KEY_END:
            return &cmdscrolltobottom;
        case KEY_PPAGE:
        case '\031': /* ctrl-Y */
            return &cmdscrolluppage;
        case KEY_NPAGE:
        case '\026': /* ctrl-V */
            return &cmdscrolldownpage;
    }
    return NULL;
}

/* Keys for "hit any key to page" mode. */
static command_t *commands_textbuffer_paging(int key)
{
    static command_t cmdscrolldownpage = { gcmd_buffer_scroll, gcmd_DownPage };

    return &cmdscrolldownpage;
}

/* Keys for char input in a text buffer window. */
static command_t *commands_textbuffer_char(int key)
{
    static command_t cmdv = { gcmd_buffer_accept_key, -1 };

    return &cmdv;
}

/* Keys for line input in a text buffer window. */
static command_t *commands_textbuffer_line(window_textbuffer_t *dwin, int key)
{
    static command_t cmdacceptline = { gcmd_buffer_accept_line, 0 };
    static command_t cmdacceptlineterm = { gcmd_buffer_accept_line, -1 };
    static command_t cmdinsert = { gcmd_buffer_insert_key, -1 };
    static command_t cmdmoveleft = { gcmd_buffer_move_cursor, gcmd_Left };
    static command_t cmdmoveright = { gcmd_buffer_move_cursor, gcmd_Right };
    static command_t cmdmoveleftend = { gcmd_buffer_move_cursor, gcmd_LeftEnd };
    static command_t cmdmoverightend = { gcmd_buffer_move_cursor, gcmd_RightEnd };
    static command_t cmddelete = { gcmd_buffer_delete, gcmd_Delete };
    static command_t cmddeletenext = { gcmd_buffer_delete, gcmd_DeleteNext };
    static command_t cmdkillinput = { gcmd_buffer_delete, gcmd_KillInput };
    static command_t cmdkillline = { gcmd_buffer_delete, gcmd_KillLine };
    static command_t cmdhistoryprev = { gcmd_buffer_history, gcmd_Up };
    static command_t cmdhistorynext = { gcmd_buffer_history, gcmd_Down };

    if (key >= 32 && key < 256 && key != '\177') 
        return &cmdinsert;
    switch (key) {
        case KEY_ENTER:
        case '\012': /* ctrl-J */
        case '\015': /* ctrl-M */
            return &cmdacceptline;
        case KEY_LEFT:
        case '\002': /* ctrl-B */
            return &cmdmoveleft;
        case KEY_RIGHT:
        case '\006': /* ctrl-F */
            return &cmdmoveright;
        case KEY_HOME:
        case '\001': /* ctrl-A */
            return &cmdmoveleftend;
        case KEY_END:
        case '\005': /* ctrl-E */
            return &cmdmoverightend;
        case '\177': /* delete */
        case '\010': /* backspace */
        case KEY_BACKSPACE:
        case KEY_DC:
            return &cmddelete;
        case '\004': /* ctrl-D */
            return &cmddeletenext;
        case '\013': /* ctrl-K */
            return &cmdkillline;
        case '\025': /* ctrl-U */
            return &cmdkillinput;
        case KEY_UP:
        case '\020': /* ctrl-P */
            return &cmdhistoryprev;
        case KEY_DOWN:
        case '\016': /* ctrl-N */
            return &cmdhistorynext;

        case '\033': /* escape */
            if (dwin->intermkeys & 0x10000)
                return &cmdacceptlineterm;
            break;
#ifdef KEY_F
        case KEY_F(1):
            if (dwin->intermkeys & (1<<1))
                return &cmdacceptlineterm;
            break;
        case KEY_F(2):
            if (dwin->intermkeys & (1<<2))
                return &cmdacceptlineterm;
            break;
        case KEY_F(3):
            if (dwin->intermkeys & (1<<3))
                return &cmdacceptlineterm;
            break;
        case KEY_F(4):
            if (dwin->intermkeys & (1<<4))
                return &cmdacceptlineterm;
            break;
        case KEY_F(5):
            if (dwin->intermkeys & (1<<5))
                return &cmdacceptlineterm;
            break;
        case KEY_F(6):
            if (dwin->intermkeys & (1<<6))
                return &cmdacceptlineterm;
            break;
        case KEY_F(7):
            if (dwin->intermkeys & (1<<7))
                return &cmdacceptlineterm;
            break;
        case KEY_F(8):
            if (dwin->intermkeys & (1<<8))
                return &cmdacceptlineterm;
            break;
        case KEY_F(9):
            if (dwin->intermkeys & (1<<9))
                return &cmdacceptlineterm;
            break;
        case KEY_F(10):
            if (dwin->intermkeys & (1<<10))
                return &cmdacceptlineterm;
            break;
        case KEY_F(11):
            if (dwin->intermkeys & (1<<11))
                return &cmdacceptlineterm;
            break;
        case KEY_F(12):
            if (dwin->intermkeys & (1<<12))
                return &cmdacceptlineterm;
            break;
#endif /* KEY_F */
    }
    return NULL;
}

/* Check to see if key is bound to anything in the given window.
    First check for char or line input bindings, then general
    bindings. */
static command_t *commands_window(window_t *win, int key)
{
    command_t *cmd = NULL;
    
    switch (win->type) {
        case wintype_TextGrid: {
            window_textgrid_t *dwin = win->data;
            cmd = commands_textgrid(key);
            if (!cmd) {
                if (win->line_request)
                    cmd = commands_textgrid_line(dwin, key);
                else if (win->char_request)
                    cmd = commands_textgrid_char(key);
            }
            }
            break;
        case wintype_TextBuffer: {
            window_textbuffer_t *dwin = win->data;
            cmd = commands_textbuffer(key);
            if (!cmd) {
                if (dwin->lastseenline < dwin->numlines - dwin->height) {
                    cmd = commands_textbuffer_paging(key);
                }
                if (!cmd) {
                    if (win->line_request)
                        cmd = commands_textbuffer_line(dwin, key);
                    else if (win->char_request)
                        cmd = commands_textbuffer_char(key);
                }
            }
            }
            break;
    }
    
    return cmd;
}

/* Return a string describing a given key. This (sometimes) uses a
    static buffer, which is overwritten with each call. */
static char *key_to_name(int key)
{
    static char kbuf[32];
    
    if (key >= 32 && key < 256) {
        if (key == 127) {
            return "delete";
        }
        kbuf[0] = key;
        kbuf[1] = '\0';
        return kbuf;
    }

    switch (key) {
        case '\t':
            return "tab";
        case '\033':
            return "escape";
        case KEY_DOWN:
            return "down-arrow";
        case KEY_UP:
            return "up-arrow";
        case KEY_LEFT:
            return "left-arrow";
        case KEY_RIGHT:
            return "right-arrow";
        case KEY_HOME:
            return "home";
        case KEY_BACKSPACE:
            return "backspace";
        case KEY_DC:
            return "delete-char";
        case KEY_IC:
            return "insert-char";
        case KEY_NPAGE:
            return "page-down";
        case KEY_PPAGE:
            return "page-up";
        case KEY_ENTER:
            return "enter";
        case KEY_END:
            return "end";
        case KEY_HELP:
            return "help";
#ifdef KEY_F
        case KEY_F(1):
            return "func-1";
        case KEY_F(2):
            return "func-2";
        case KEY_F(3):
            return "func-3";
        case KEY_F(4):
            return "func-4";
        case KEY_F(5):
            return "func-5";
        case KEY_F(6):
            return "func-6";
        case KEY_F(7):
            return "func-7";
        case KEY_F(8):
            return "func-8";
        case KEY_F(9):
            return "func-9";
        case KEY_F(10):
            return "func-10";
        case KEY_F(11):
            return "func-11";
        case KEY_F(12):
            return "func-12";
#endif /* KEY_F */
    }

    if (key >= 0 && key < 32) {
        sprintf(kbuf, "ctrl-%c", '@'+key);
        return kbuf;
    }

    return "unknown-key";
}

glui32 gli_input_from_native(int key)
{
  glui32 arg = 0;

  /* convert from curses.h key codes to Glk, if necessary. */
  switch (key) {
  case '\t': 
    arg = keycode_Tab;
    break;
  case '\033':
    arg = keycode_Escape;
    break;
  case KEY_DOWN:
    arg = keycode_Down;
    break;
  case KEY_UP:
    arg = keycode_Up;
    break;
  case KEY_LEFT:
    arg = keycode_Left;
    break;
  case KEY_RIGHT:
    arg = keycode_Right;
    break;
  case KEY_HOME:
    arg = keycode_Home;
    break;
  case '\177': /* delete */
  case '\010': /* backspace */
  case KEY_BACKSPACE:
  case KEY_DC:
    arg = keycode_Delete;
    break;
  case KEY_NPAGE:
    arg = keycode_PageDown;
    break;
  case KEY_PPAGE:
    arg = keycode_PageUp;
    break;
  case KEY_ENTER:
  case '\012': /* ctrl-J */
  case '\015': /* ctrl-M */
    arg = keycode_Return;
    break;
  case KEY_END:
    arg = keycode_End;
    break;
#ifdef KEY_F
  case KEY_F(1):
    arg = keycode_Func1;
    break;
  case KEY_F(2):
    arg = keycode_Func2;
    break;
  case KEY_F(3):
    arg = keycode_Func3;
    break;
  case KEY_F(4):
    arg = keycode_Func4;
    break;
  case KEY_F(5):
    arg = keycode_Func5;
    break;
  case KEY_F(6):
    arg = keycode_Func6;
    break;
  case KEY_F(7):
    arg = keycode_Func7;
    break;
  case KEY_F(8):
    arg = keycode_Func8;
    break;
  case KEY_F(9):
    arg = keycode_Func9;
    break;
  case KEY_F(10):
    arg = keycode_Func10;
    break;
  case KEY_F(11):
    arg = keycode_Func11;
    break;
  case KEY_F(12):
    arg = keycode_Func12;
    break;
#endif /* KEY_F */
  default:
    if (key < 0 || key >= 256) {
      arg = keycode_Unknown;
    }
    else {
#ifdef OPT_NATIVE_LATIN_1
      arg = key;
#else /* OPT_NATIVE_LATIN_1 */
      arg = char_from_native_table[key];
      if (!arg && key != '\0')
        arg = keycode_Unknown;
#endif /* OPT_NATIVE_LATIN_1 */
    }
    break;
  }

  return arg;
}

/* Handle a keystroke. This is called from glk_select() whenever a
    key is hit. */
void gli_input_handle_key(int key)
{
    command_t *cmd = NULL;
    window_t *win = NULL;

    /* First, see if the key has a general binding. */
    if (!cmd) {
        cmd = commands_always(key);
        if (cmd)
            win = NULL;
    }

    /* If not, see if the key is bound in the focus window. */
    if (!cmd && gli_focuswin) {
        cmd = commands_window(gli_focuswin, key);
        if (cmd)
            win = gli_focuswin;
    }
    
    /* If not, see if there's some other window which has a binding for
        the key; if so, set the focus there. */
    if (!cmd && gli_rootwin) {
        window_t *altwin = gli_focuswin;
        command_t *altcmd = NULL;
        do {
            altwin = gli_window_iterate_treeorder(altwin);
            if (altwin && altwin->type != wintype_Pair) {
                altcmd = commands_window(altwin, key);
                if (altcmd)
                    break;
            }
        } while (altwin != gli_focuswin);
        if (altwin != gli_focuswin && altcmd) {
            cmd = altcmd;
            win = altwin;
            gli_focuswin = win; /* set the focus */
        }
    }
    
    if (cmd) {
        /* We found a binding. Run it. */
        glui32 arg;
        if (cmd->arg == -1) {
            arg = (glui32)key;
        }
        else {
            arg = cmd->arg;
        }
        (*cmd->func)(win, arg);
    }
    else {
        char buf[256];
        char *kbuf = key_to_name(key);
        sprintf(buf, "The key <%s> is not currently defined.", kbuf);
        gli_msgline(buf);
    }
}

/* Pick a window which might want input. This is called at the beginning
    of glk_select(). */
void gli_input_guess_focus()
{
    window_t *altwin;
    
    if (gli_focuswin 
        && (gli_focuswin->line_request || gli_focuswin->char_request)) {
        return;
    }
    
    altwin = gli_focuswin;
    do {
        altwin = gli_window_iterate_treeorder(altwin);
        if (altwin 
            && (altwin->line_request || altwin->char_request)) {
            break;
        }
    } while (altwin != gli_focuswin);
    
    if (gli_focuswin != altwin)
        gli_focuswin = altwin;
}

