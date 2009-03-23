/* gtstyle.c: Style formatting hints.
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include "glk.h"
#include "glkterm.h"

/* ### None of these functions are particularly implemented yet. */

void glk_stylehint_set(uint32 wintype, uint32 styl, uint32 hint, 
    uint32 val)
{
}

void glk_stylehint_clear(uint32 wintype, uint32 styl, uint32 hint)
{
}

uint32 glk_style_distinguish(winid_t win, uint32 styl1, uint32 styl2)
{
    return FALSE;
}

uint32 glk_style_measure(winid_t win, uint32 styl, uint32 hint, 
    uint32 *result)
{
    return FALSE;
}
