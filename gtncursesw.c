#include "gtoption.h"

#ifdef LOCAL_NCURSESW

#include <curses.h>
#include <stdlib.h>
#include <wchar.h>
#include <memory.h>
/* only needed for local_* prototypes */
#include "glk.h"
#include "glkterm.h"

int local_get_wch (wint_t *ch)
{
    int i;
    int status = 0;
    char *buffer = (char *) calloc (MB_CUR_MAX + 1, sizeof (char));
    mbstate_t state;
    
    for ( i = 0; status != ERR && i < MB_CUR_MAX; ++ i ) {
        status = getch();
        if ( status == ERR ) {
            break;
        }
        if ( ((unsigned) status) >= 0x100 ) {
            /* returned a function key */
            *ch = status;
            status = KEY_CODE_YES;
            free (buffer);
            return status;
        }
        buffer[i] = status;
        memset (&state, '\0', sizeof (state));
        status = mbrlen (buffer, i + 1, &state);
        switch (status) {
            case -2: /* continue reading */
                status = i + 1 < MB_CUR_MAX ? OK : ERR;
                break;
            case -1: /* abort */
                status = ERR;
                break;
            default: /* got a character */
                memset (&state, '\0', sizeof (state));
                status = mbrtowc ((wchar_t *) ch, buffer, i + 1, &state);
                status = OK;
                /* This is just to break the loop */
                i = MB_CUR_MAX;
                break;
        }
        
    }

    free (buffer);
    return status;
}

int local_addnwstr(const wchar_t *wstr, int n)
{
    int i;
    int status = OK;
    size_t size = 0;
    char *buffer = (char *) calloc (MB_CUR_MAX + 1, sizeof (char));
    mbstate_t state;
    
    for ( i = 0; status != ERR && i < n && wstr[i] != L'\0'; ++ i ) {
        memset (&state, '\0', sizeof (state));
        size = wcrtomb (buffer, wstr[i], &state);
        if ( size == (size_t) -1 ) {
            status = ERR;
        }
	else {
            addnstr(buffer, size);
        }
    }
    
    free (buffer);
    return status;
}

int local_addwstr(const wchar_t *wstr)
{
    return local_addnwstr(wstr, wcslen(wstr));
}

int local_mvaddnwstr(int y, int x, const wchar_t *wstr, int n)
{
    move (y, x);
    
    return local_addnwstr(wstr, n);
}

#else /* LOCAL_NCURSESW */

#define _XOPEN_SOURCE_EXTENDED /* ncursesw *wch* and *wstr* functions */
#include <ncursesw/ncurses.h>

int local_get_wch (wint_t *ch)
{
    return get_wch(ch);
}

int local_addwstr(const wchar_t *wstr)
{
    return addwstr(wstr);
}

int local_mvaddnwstr(int y, int x, const wchar_t *wstr, int n)
{
    return mvaddnwstr(y, x, wstr, n);
}

int local_addnwstr(const wchar_t *wstr, int n)
{
    return addnwstr(wstr, n);
}

#endif /* LOCAL_NCURSESW */
