/* gtmessin.c: Special input at the bottom of the screen
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

/* This is for when the library wants to prompt for some input, but not
    in a window. It is currently used in two places: 
        glk_fileref_create_by_prompt(), when prompting for a filename;
        glk_exit(), for "hit any key to exit".
   Sadly, this input is not done through the program's event loop, which
    means timer and window-rearrange events do not occur during these
    operations.
*/

typedef struct inline_struct {
    char *prompt;
    char *buf;
    int maxlen;
    int len;
    
    int orgx, orgy;
    int curs;
    
    int done;
    int accept;
} inline_t;

#define LEFT_MARGIN (1)

static void handle_key(inline_t *lin, int key);
static void update_text(inline_t *lin);

int gli_msgin_getchar(char *prompt, int hilite)
{
    int orgx, orgy;
    int key;

    gli_windows_update();
    gli_windows_set_paging(TRUE);
    
    if (!prompt)
        prompt = "";

    orgx = LEFT_MARGIN + strlen(prompt);

    /* If the bottom line is reserved for messages, great; clear the message
        line and do input there. If not, we'll have to wipe the bottom line
        of the bottommost game window, and redraw it later. */
    if (pref_messageline) {
        orgy = content_box.bottom;
        gli_msgline(NULL);
    }
    else {
        orgy = content_box.bottom-1;
        move(orgy, 0);
        clrtoeol();
    }

    move(orgy, LEFT_MARGIN);
    if (hilite)
        attron(A_REVERSE);
    addstr(prompt);
    if (hilite)
        attrset(0);

    move(orgy, orgx);
    refresh();

    key = ERR;
    while (key == ERR) {
        key = getch();
    }
    
    if (pref_messageline) {
        gli_msgline(NULL);
    }
    else {
        move(orgy, 0);
        clrtoeol();
        /* We have to redraw everything, unfortunately, to fix the
            last line. */
        gli_windows_update();
    }
    
    return key;
}

int gli_msgin_getline(char *prompt, char *buf, int maxlen, int *length)
{
    inline_t indata; /* just allocate it on the stack */
    inline_t *lin = &indata;
    int needrefresh;
    
    gli_windows_update();
    gli_windows_set_paging(TRUE);

    if (!prompt)
        prompt = "";
    
    lin->done = FALSE;
    lin->accept = FALSE;
    
    lin->prompt = prompt;
    lin->buf = buf;
    lin->maxlen = maxlen;
    lin->len = *length;
    lin->curs = lin->len;
    
    lin->orgx = LEFT_MARGIN + strlen(prompt);
    
    /* See note in gli_msgin_getchar(). */
    if (pref_messageline) {
        lin->orgy = content_box.bottom;
        gli_msgline(NULL);
    }
    else {
        lin->orgy = content_box.bottom-1;
        move(lin->orgy, 0);
        clrtoeol();
    }
    
    move(lin->orgy, LEFT_MARGIN);
    addstr(lin->prompt);
    update_text(lin);
    
    needrefresh = TRUE;
    
    while (!lin->done) {
        int key;
        
        move(lin->orgy, lin->orgx + lin->curs);
        if (needrefresh) {
            refresh();
            needrefresh = FALSE;
        }

        key = getch();
        
        if (key != ERR) {
            handle_key(lin, key);
            needrefresh = TRUE;
            continue;
        }
    }
    
    if (pref_messageline) {
        gli_msgline(NULL);
    }
    else {
        move(lin->orgy, 0);
        clrtoeol();
        /* We have to redraw everything, unfortunately, to fix the
            last line. */
        gli_windows_update();
    }

    *length = lin->len;
    return lin->accept;
}

static void update_text(inline_t *lin)
{
    int ix;
    
    move(lin->orgy, lin->orgx);
    for (ix=0; ix<lin->len; ix++) {
        addch(lin->buf[ix]);
    }
    clrtoeol();
}

static void handle_key(inline_t *lin, int key)
{
    char *buf = lin->buf; /* cache */
    
    switch (key) {
    
        case KEY_ENTER:
        case '\012': /* ctrl-J */
        case '\015': /* ctrl-M */
            lin->accept = TRUE;
            lin->done = TRUE;
            break;
            
        case '\007': /* ctrl-G */
        case '\033': /* escape */
            lin->accept = FALSE;
            lin->done = TRUE;
            break;
            
        case KEY_LEFT:
        case '\002': /* ctrl-B */
            if (lin->curs > 0) {
                lin->curs--;
            }
            break;
            
        case KEY_RIGHT:
        case '\006': /* ctrl-F */
            if (lin->curs < lin->len) {
                lin->curs++;
            }
            break;

        case KEY_HOME:
        case '\001': /* ctrl-A */
            if (lin->curs > 0) {
                lin->curs = 0;
            }
            break;
            
        case KEY_END:
        case '\005': /* ctrl-E */
            if (lin->curs < lin->len) {
                lin->curs = lin->len;
            }
            break;

        case '\177': /* delete */
        case '\010': /* backspace */
        case KEY_BACKSPACE:
        case KEY_DC:
            if (lin->curs > 0) {
                if (lin->curs < lin->len) {
                    memmove(buf+(lin->curs-1), buf+(lin->curs),
                        (lin->len - lin->curs) * sizeof(char));
                }
                lin->len--;
                lin->curs--;
                update_text(lin);
            }
            break;

        case '\004': /* ctrl-D */
            if (lin->curs < lin->len) {
                if ((lin->curs+1) < lin->len) {
                    memmove(buf+(lin->curs), buf+(lin->curs+1),
                        (lin->len - (lin->curs+1)) * sizeof(char));
                }
                lin->len--;
                update_text(lin);
            }
            break;

        case '\013': /* ctrl-K */
            if (lin->curs < lin->len) {
                lin->len = lin->curs;
                update_text(lin);
            }
            break;
            
        case '\025': /* ctrl-U */
            if (lin->curs > 0) {
                lin->len = 0;
                lin->curs = 0;
                update_text(lin);
            }
            break;
        
        default: /* everything else */
            if (key >= 32 && key < 256) {
                if (lin->len < lin->maxlen) {
                    if (lin->curs < lin->len) {
                        memmove(buf+(lin->curs+1), buf+(lin->curs), 
                            (lin->len - lin->curs) * sizeof(char));
                    }
                    lin->len++;
                    buf[lin->curs] = key;
                    lin->curs++;
                    update_text(lin);
                }
                break;
            }
            break;
    
    }
}
