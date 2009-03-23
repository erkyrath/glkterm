/* gtmessin.c: Special input at the bottom of the screen
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#define _XOPEN_SOURCE /* wcwidth */
#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
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
    wchar_t *prompt;
    wchar_t *buf;
    int maxlen;
    int len;
    
    int orgx, orgy;
    int curs;
    
    int done;
    int accept;
} inline_t;

#define LEFT_MARGIN (1)

static void handle_key(inline_t *lin, glui32 key);
static void update_text(inline_t *lin);

glui32 gli_msgin_getchar(wchar_t *prompt, int hilite)
{
    int orgx, orgy;
    glui32 key;
    int status;

    gli_windows_update();
    gli_windows_set_paging(TRUE);
    
    if (!prompt)
        prompt = L"";

    orgx = LEFT_MARGIN + wcswidth(prompt, wcslen(prompt));

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
    local_addwstr(prompt);
    if (hilite)
        attrset(0);

    move(orgy, orgx);
    refresh();

    status = ERR;
    while (status == ERR) {
        status = gli_get_key(&key);
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

int gli_msgin_getline(wchar_t *prompt, wchar_t *buf, int maxlen, int *length)
{
    inline_t indata; /* just allocate it on the stack */
    inline_t *lin = &indata;
    int needrefresh;
    
    gli_windows_update();
    gli_windows_set_paging(TRUE);

    if (!prompt)
        prompt = L"";
    
    lin->done = FALSE;
    lin->accept = FALSE;
    
    lin->prompt = prompt;
    lin->buf = buf;
    lin->maxlen = maxlen;
    lin->len = *length;
    lin->curs = lin->len;
    
    lin->orgx = LEFT_MARGIN + wcswidth(prompt, wcslen(prompt));
    
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
    local_addwstr(lin->prompt);
    update_text(lin);
    
    needrefresh = TRUE;
    
    while (!lin->done) {
        wint_t key;
        glui32 key32;
        int status;
        
        move(lin->orgy, lin->orgx + wcswidth(lin->buf, lin->curs));
        if (needrefresh) {
            refresh();
            needrefresh = FALSE;
        }

        status = gli_get_key(&key32);
        key = key32;
        
        if (status != ERR) {
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
    move(lin->orgy, lin->orgx);
    local_addnwstr(lin->buf, lin->len);
    clrtoeol();
}

static void handle_key(inline_t *lin, glui32 key)
{
    wchar_t *buf = lin->buf; /* cache */
    
    switch (key) {
    
        case keycode_Return:
            lin->accept = TRUE;
            lin->done = TRUE;
            break;
            
        case '\007': /* ctrl-G */
        case keycode_Escape: /* escape */
            lin->accept = FALSE;
            lin->done = TRUE;
            break;
            
        case keycode_Left:
        case '\002': /* ctrl-B */
            if (lin->curs > 0) {
                lin->curs--;
            }
            break;
            
        case keycode_Right:
        case '\006': /* ctrl-F */
            if (lin->curs < lin->len) {
                lin->curs++;
            }
            break;

        case keycode_Home:
        case '\001': /* ctrl-A */
            if (lin->curs > 0) {
                lin->curs = 0;
            }
            break;
            
        case keycode_End:
        case '\005': /* ctrl-E */
            if (lin->curs < lin->len) {
                lin->curs = lin->len;
            }
            break;

        case keycode_Delete:
            if (lin->curs > 0) {
                if (lin->curs < lin->len) {
                    memmove(buf+(lin->curs-1), buf+(lin->curs),
                        (lin->len - lin->curs) * sizeof(wchar_t));
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
                        (lin->len - (lin->curs+1)) * sizeof(wchar_t));
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
            if ( (! gli_bad_latin_key(key)) && iswprint(glui32_to_wchar(key))) {
                if (lin->len < lin->maxlen) {
                    if (lin->curs < lin->len) {
                        memmove(buf+(lin->curs+1), buf+(lin->curs), 
                            (lin->len - lin->curs) * sizeof(wchar_t));
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
