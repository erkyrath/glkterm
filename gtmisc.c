/* gtmisc.c: Miscellaneous functions
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"

static unsigned char char_tolower_table[256];
static unsigned char char_toupper_table[256];
unsigned char char_printable_table[256];
unsigned char char_typable_table[256];
static unsigned char char_A0_FF_typable[6*16] = OPT_AO_FF_TYPABLE;
#ifndef OPT_NATIVE_LATIN_1
static unsigned char char_A0_FF_output[6*16] = OPT_AO_FF_OUTPUT;
unsigned char char_from_native_table[256];
unsigned char char_to_native_table[256];
#endif /* OPT_NATIVE_LATIN_1 */

static char *char_A0_FF_to_ascii[6*16] = {
    " ", "!", "c", "Lb", NULL, "Y", "|", NULL,
    NULL, "(C)", NULL, "<<", NULL, "-", "(R)", NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "*",
    NULL, NULL, NULL, ">>", "1/4", "1/2", "3/4", "?",
    "A", "A", "A", "A", "A", "A", "AE", "C",
    "E", "E", "E", "E", "I", "I", "I", "I",
    NULL, "N", "O", "O", "O", "O", "O", "x",
    "O", "U", "U", "U", "U", "Y", NULL, "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c",
    "e", "e", "e", "e", "i", "i", "i", "i",
    NULL, "n", "o", "o", "o", "o", "o", "/",
    "o", "u", "u", "u", "u", "y", NULL, "y",
};

/* Set up things. This is called from main(). */
void gli_initialize_misc()
{
    int ix;
    
    /* Initialize the to-uppercase and to-lowercase tables. These should
        *not* be localized to a platform-native character set! They are
        intended to work on Latin-1 data, and the code below correctly
        sets up the tables for that character set. */
    
    for (ix=0; ix<256; ix++) {
        char_toupper_table[ix] = ix;
        char_tolower_table[ix] = ix;
    }
    for (ix=0; ix<256; ix++) {
        int lower_equiv;
        if (ix >= 'A' && ix <= 'Z') {
            lower_equiv = ix + ('a' - 'A');
        }
        else if (ix >= 0xC0 && ix <= 0xDE && ix != 0xD7) {
            lower_equiv = ix + 0x20;
        }
        else {
            lower_equiv = 0;
        }
        if (lower_equiv) {
            char_tolower_table[ix] = lower_equiv;
            char_toupper_table[lower_equiv] = ix;
        }
    }

#ifndef OPT_NATIVE_LATIN_1
    for (ix=0; ix<256; ix++) {
        if (ix <= 0x7E)
            char_from_native_table[ix] = ix;
        else
            char_from_native_table[ix] = 0;
    }
#endif /* OPT_NATIVE_LATIN_1 */

    for (ix=0; ix<256; ix++) {
        unsigned char native_equiv;
        int cantype, canprint;
        native_equiv = ix;
        if (ix < 0x20) {
            /* Many control characters are untypable, for many reasons. */
            if (ix == '\t' || ix == '\014'  /* reserved by the input system */
#ifdef OPT_USE_SIGNALS
                || ix == '\003' || ix == '\032' /* interrupt/suspend signals */
#endif
                || ix == '\010'             /* parsed as keycode_Delete */
                || ix == '\012' || ix == '\015' /* parsed as keycode_Return */
                || ix == '\033')            /* parsed as keycode_Escape */
                cantype = FALSE;
            else
                cantype = TRUE;
            /* The newline is printable, but no other control characters. */
            if (ix == '\012')
                canprint = TRUE;
            else
                canprint = FALSE;
        }
        else if (ix <= 0x7E) {
            cantype = TRUE;
            canprint = TRUE;
        }
        else if (ix < 0xA0) {
            cantype = FALSE;
            canprint = FALSE;
        }
        else {
            cantype = char_A0_FF_typable[ix - 0xA0];
#ifdef OPT_NATIVE_LATIN_1
            canprint = TRUE;
#else /* OPT_NATIVE_LATIN_1 */
            native_equiv = char_A0_FF_output[ix - 0xA0];
            cantype = cantype && native_equiv; /* If it can't be printed exactly, it certainly
                can't be typed. */
            canprint = (native_equiv != 0);
#endif /* OPT_NATIVE_LATIN_1 */
        }
        char_typable_table[ix] = cantype;
        char_printable_table[ix] = canprint;
#ifndef OPT_NATIVE_LATIN_1
        char_to_native_table[ix] = native_equiv;
        if (native_equiv)
            char_from_native_table[native_equiv] = ix;
#endif /* OPT_NATIVE_LATIN_1 */
    }

#ifndef OPT_NATIVE_LATIN_1
    char_from_native_table[0] = '\0'; /* The little dance above misses this
        entry, for dull reasons. */
#endif /* OPT_NATIVE_LATIN_1 */
}

void glk_exit()
{   
    gli_msgin_getchar("Hit any key to exit.", TRUE);

    gli_streams_close_all();

    endwin();
    putchar('\n');
    exit(0);
}

void glk_set_interrupt_handler(void (*func)(void))
{
    gli_interrupt_handler = func;
}

void glk_tick()
{
    /* Nothing to do here. */
}

unsigned char glk_char_to_lower(unsigned char ch)
{
    return char_tolower_table[ch];
}

unsigned char glk_char_to_upper(unsigned char ch)
{
    return char_toupper_table[ch];
}

char *gli_ascii_equivalent(unsigned char ch)
{
    static char buf[5];
    
    if (ch >= 0xA0 && char_A0_FF_to_ascii[ch - 0xA0]) {
        return char_A0_FF_to_ascii[ch - 0xA0];
    }
    
    buf[0] = '\\';
    buf[1] = '0' + ((ch >> 6) & 7);
    buf[2] = '0' + ((ch >> 3) & 7);
    buf[3] = '0' + ((ch) & 7);
    buf[4] = '\0';
    
    return buf;
}

#ifdef NO_MEMMOVE

void *memmove(void *destp, void *srcp, int n)
{
    char *dest = (char *)destp;
    char *src = (char *)srcp;
    
    if (dest < src) {
        for (; n > 0; n--) {
            *dest = *src;
            dest++;
            src++;
        }
    }
    else if (dest > src) {
        src += n;
        dest += n;
        for (; n > 0; n--) {
            dest--;
            src--;
            *dest = *src;
        }
    }
    
    return destp;
}

#endif /* NO_MEMMOVE */
