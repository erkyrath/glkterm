/* gtmessag.c: The message line at the bottom of the screen
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"

/* Nothing fancy here. We store a string, and print it on the bottom line.
    If pref_messageline is FALSE, none of these functions do anything. */

static wchar_t *msgbuf = NULL;
static int msgbuflen = 0;
static int msgbuf_size = 0;

void gli_msgline_warning(wchar_t *msg)
{
    wchar_t buf[256];
    
    if (!pref_messageline)
        return;
    
    buf[0] = L'\0';
    wcsncat(buf, L"Glk library error: ", 256);
    int l = wcslen(buf);
    wcsncat(buf + l, msg, 256 - l);
    gli_msgline(buf);
}

void gli_msgline(wchar_t *msg)
{
    int len;
    
    if (!pref_messageline)
        return;
        
    if (!msg) 
        msg = L"";
    
    len = wcslen(msg);
    if (!msgbuf) {
        msgbuf_size = len+80;
        msgbuf = (wchar_t *)malloc(msgbuf_size * sizeof(wchar_t));
    }
    else if (len+1 > msgbuf_size) {
        while (len+1 > msgbuf_size)
            msgbuf_size *= 2;
        msgbuf = (wchar_t *)realloc(msgbuf, msgbuf_size * sizeof(wchar_t));
    }

    if (!msgbuf)
        return;
    
    wcscpy(msgbuf, msg);
    msgbuflen = len;
    
    gli_msgline_redraw();
}

void gli_msgline_redraw()
{
    if (!pref_messageline)
        return;
        
    if (msgbuflen == 0) {
        move(content_box.bottom, 0);
        clrtoeol();
    }
    else {
        int len;
        
        move(content_box.bottom, 0);
        local_addwstr(L"  ");
        attron(A_REVERSE);
        if (msgbuflen > content_box.right-3)
            len = content_box.right-3;
        else
            len = msgbuflen;
        local_addnwstr(msgbuf, len);
        attrset(0);
        clrtoeol();
    }
}
