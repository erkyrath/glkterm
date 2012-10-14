/* gtmisc.c: Miscellaneous functions
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

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock) = NULL;
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode) = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, 
    gidispatch_rock_t objrock) = NULL;

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

void gidispatch_set_object_registry(
    gidispatch_rock_t (*regi)(void *obj, glui32 objclass), 
    void (*unregi)(void *obj, glui32 objclass, gidispatch_rock_t objrock))
{
    window_t *win;
    stream_t *str;
    fileref_t *fref;
    
    gli_register_obj = regi;
    gli_unregister_obj = unregi;
    
    if (gli_register_obj) {
        /* It's now necessary to go through all existing objects, and register
            them. */
        for (win = glk_window_iterate(NULL, NULL); 
            win;
            win = glk_window_iterate(win, NULL)) {
            win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);
        }
        for (str = glk_stream_iterate(NULL, NULL); 
            str;
            str = glk_stream_iterate(str, NULL)) {
            str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);
        }
        for (fref = glk_fileref_iterate(NULL, NULL); 
            fref;
            fref = glk_fileref_iterate(fref, NULL)) {
            fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);
        }
    }
}

void gidispatch_set_retained_registry(
    gidispatch_rock_t (*regi)(void *array, glui32 len, char *typecode), 
    void (*unregi)(void *array, glui32 len, char *typecode, 
        gidispatch_rock_t objrock))
{
    gli_register_arr = regi;
    gli_unregister_arr = unregi;
}

gidispatch_rock_t gidispatch_get_objrock(void *obj, glui32 objclass)
{
    switch (objclass) {
        case gidisp_Class_Window:
            return ((window_t *)obj)->disprock;
        case gidisp_Class_Stream:
            return ((stream_t *)obj)->disprock;
        case gidisp_Class_Fileref:
            return ((fileref_t *)obj)->disprock;
        default: {
            gidispatch_rock_t dummy;
            dummy.num = 0;
            return dummy;
        }
    }
}

void gidispatch_set_autorestore_registry(
    long (*locatearr)(void *array, glui32 len, char *typecode,
        gidispatch_rock_t objrock, int *elemsizeref),
    gidispatch_rock_t (*restorearr)(long bufkey, glui32 len,
        char *typecode, void **arrayref))
{
    /* GlkTerm is not able to serialize its UI state. Therefore, it
       does not have the capability of autosaving and autorestoring.
       Therefore, it will never call these hooks. Therefore, we ignore
       them and do nothing here. */
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
