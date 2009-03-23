/* gtstyle.c: Style formatting hints.
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "gtw_grid.h"
#include "gtw_buf.h"

/* This version of the library doesn't accept style hints. */

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, 
    glsi32 val)
{
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
}

glui32 glk_style_distinguish(window_t *win, glui32 styl1, glui32 styl2)
{
    chtype *styleattrs;

    if (!win) {
        gli_strict_warning("style_distinguish: invalid ref");
        return FALSE;
    }
    
    if (styl1 >= style_NUMSTYLES || styl2 >= style_NUMSTYLES)
        return FALSE;
    
    switch (win->type) {
        case wintype_TextBuffer:
            styleattrs = win_textbuffer_styleattrs;
            break;
        case wintype_TextGrid:
            styleattrs = win_textgrid_styleattrs;
            break;
        default:
            return FALSE;
    }
    
    /* styleattrs is an array of chtype values, as defined in curses.h. */
    
    if (styleattrs[styl1] != styleattrs[styl2])
        return TRUE;
    
    return FALSE;
}

glui32 glk_style_measure(window_t *win, glui32 styl, glui32 hint, 
    glui32 *result)
{
    chtype *styleattrs;
    glui32 dummy;

    if (!win) {
        gli_strict_warning("style_measure: invalid ref");
        return FALSE;
    }
    
    if (styl >= style_NUMSTYLES || hint >= stylehint_NUMHINTS)
        return FALSE;
    
    switch (win->type) {
        case wintype_TextBuffer:
            styleattrs = win_textbuffer_styleattrs;
            break;
        case wintype_TextGrid:
            styleattrs = win_textgrid_styleattrs;
            break;
        default:
            return FALSE;
    }
    
    if (!result)
        result = &dummy;
    
    switch (hint) {
        case stylehint_Indentation:
        case stylehint_ParaIndentation:
            *result = 0;
            return TRUE;
        case stylehint_Justification:
            *result = stylehint_just_LeftFlush;
            return TRUE;
        case stylehint_Size:
            *result = 1;
            return TRUE;
        case stylehint_Weight:
            *result = ((styleattrs[styl] & A_BOLD) != 0);
            return TRUE;
        case stylehint_Oblique:
            *result = ((styleattrs[styl] & A_UNDERLINE) != 0);
            return TRUE;
        case stylehint_Proportional:
            *result = FALSE;
            return TRUE;
    }
    
    return FALSE;
}
