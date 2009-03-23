/* gtgestal.c: The Gestalt system
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include "glk.h"
#include "glkterm.h"

uint32 glk_gestalt(uint32 id, uint32 val)
{
    return glk_gestalt_ext(id, val, NULL);
}

uint32 glk_gestalt_ext(uint32 id, uint32 val, void *ptr)
{
    int ix;
    
    switch (id) {
        
        case gestalt_Version:
            return 0x00000300;
        
        case gestalt_LineInput:
            /* ### probably wrong */
            if (val >= 32 && val < 127)
                return TRUE;
            else
                return FALSE;
                
        case gestalt_CharInput: 
            /* ### probably wrong */
            if (val >= 32 && val < 127)
                return TRUE;
            else if (val == keycode_Return)
                return TRUE;
            else
                return FALSE;
        
        case gestalt_CharOutput: 
            /* ### probably wrong */
            if (val >= 32 && val < 127) {
                if (ptr)
                    *((uint32 *)ptr) = 1;
                return gestalt_CharOutput_ExactPrint;
            }
            else {
                /* Cheaply, we don't do any translation of printed
                    characters, so the output is always one character 
                    even if it's wrong. */
                if (ptr)
                    *((uint32 *)ptr) = 1;
                return gestalt_CharOutput_CannotPrint;
            }
            
        case gestalt_MouseInput: 
            return FALSE;
            
        case gestalt_Timer: 
#ifdef OPT_TIMED_INPUT
            return TRUE;
#else /* !OPT_TIMED_INPUT */
            return FALSE;
#endif /* OPT_TIMED_INPUT */
            
        default:
            return 0;

    }
}
