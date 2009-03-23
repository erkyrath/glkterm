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

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, 
    glsi32 val)
{
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
}

glui32 glk_style_distinguish(winid_t win, glui32 styl1, glui32 styl2)
{
    return FALSE;
}

glui32 glk_style_measure(winid_t win, glui32 styl, glui32 hint, 
    glui32 *result)
{
    return FALSE;
}
