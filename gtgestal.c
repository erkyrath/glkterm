/* gtgestal.c: The Gestalt system
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "glkterm.h"

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr, glui32 arrlen)
{
    static int impl;
    int ix;
    
    impl=TRUE;
    switch (id) {
        
        case gestalt_Version:
            /* This implements Glk spec version 0.7.4. */
            return 0x00000704;
        
        case gestalt_LineInput:
            if ((val >= 0 && val < 32) || val == '\177') {
                /* Control characters never appear in line input. */
                return FALSE;
            }
            if (val >= 32 && val < 256) {
                return char_typable_table[val];
            }
            return FALSE;
                
        case gestalt_CharInput: 
            if (val >= 0 && val < 256) {
                return char_typable_table[val];
            }
            if (val <= 0xFFFFFFFF && val > (0xFFFFFFFF - keycode_MAXVAL)) {
                /* Special key code. We conservatively declare that only the
                    arrow keys, return, del/backspace, and escape can be
                    typed. Function keys might work, but we can't be
                    sure they're there. */
                if (val == keycode_Left || val == keycode_Right
                    || val == keycode_Up || val == keycode_Down
                    || val == keycode_Return || val == keycode_Delete
                    || val == keycode_Escape)
                    return (val == keycode_Return || pref_typable_specials);
                else
                    return FALSE;
            }
            return FALSE;
        
        case gestalt_CharOutput: 
            if (char_printable_table[(unsigned char)val]) {
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_ExactPrint;
            }
            else {
                char *altstr = gli_ascii_equivalent((unsigned char)val);
                ix = strlen(altstr);
                if (arr && arrlen >= 1)
                    arr[0] = ix;
                if (ix == 4 && altstr[0] == '\\') {
                    /* It's a four-character octal code, "\177". */
                    return gestalt_CharOutput_CannotPrint;
                }
                else {
                    /* It's some string from char_A0_FF_to_ascii() in
                        gtmisc.c. */
                    return gestalt_CharOutput_ApproxPrint;
                }
            }
            
        case gestalt_MouseInput: 
            return FALSE;
            
        case gestalt_Timer: 
#ifdef OPT_TIMED_INPUT
            return TRUE;
#else /* !OPT_TIMED_INPUT */
            return FALSE;
#endif /* OPT_TIMED_INPUT */

        case gestalt_Graphics:
        case gestalt_GraphicsTransparency:
            return FALSE;
            
        case gestalt_DrawImage:
            return FALSE;
            
        case gestalt_Unicode:
#ifdef GLK_MODULE_UNICODE
            return TRUE;
#else
            return FALSE;
#endif /* GLK_MODULE_UNICODE */
            
        case gestalt_UnicodeNorm:
#ifdef GLK_MODULE_UNICODE_NORM
            return TRUE;
#else
            return FALSE;
#endif /* GLK_MODULE_UNICODE_NORM */
            
        case gestalt_Sound:
        case gestalt_SoundVolume:
        case gestalt_SoundNotify: 
        case gestalt_SoundMusic:
            return FALSE;
  
        case gestalt_LineInputEcho:
            return TRUE;

        case gestalt_LineTerminators:
            return pref_typable_specials;
        case gestalt_LineTerminatorKey:
            /* GlkTerm never uses the escape or function keys for anything,
               so we'll allow them to be line terminators. */
            if (!pref_typable_specials) return FALSE;
            if (val == keycode_Escape)
                return TRUE;
            if (val >= keycode_Func12 && val <= keycode_Func1)
                return TRUE;
            return FALSE;

        case gestalt_DateTime:
            return TRUE;

        case gestalt_ResourceStream:
            return TRUE;

        case 0x1400: /* gestalt_Gestalt */
            glk_gestalt_ext(val,0,NULL,0);
            return impl;

        case 0x1407: /* gestalt_CharInputExt */
            if(val=='\031' || val=='\026') return char_typable_table[val]?0x22:0;
            else return char_typable_table[val]?0x33:0;

        default:
            impl=FALSE;
            return 0;

    }
}

