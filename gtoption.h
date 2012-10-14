/* gtoption.h: Options header file
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#ifndef GTOPTION_H
#define GTOPTION_H

/* This is the only GlkTerm source file you should need to edit
    in order to compile the thing. Well, unless there's something
    funny about your OS -- which is probably inevitable. See the
    readme file for a list of OSes which GlkTerm is known to
    compile under, and what changes are appropriate.
*/

/* Options: */

#define LIBRARY_VERSION "1.0.4"
#define LIBRARY_PORT "Generic"

/* If you change this code substantially, you should change the
    LIBRARY_PORT definition to something which explains what the
    heck you did. (For example, if you release an Amiga port --
    or whatever -- and jigger the display code to make it work,
    you could change LIBRARY_PORT to "Amiga port".)
   The LIBRARY_VERSION definition should generally stay the same; 
    it tracks the version of the original GlkTerm code that I
    support. If you want to distinguish different versions of a
    port, you could change LIBRARY_PORT to "Amiga port 1.3".
   If you want to make major changes to the internals of the
    GlkTerm library, add a note to LIBRARY_VERSION, or contact
    me so that I can incorporate your changes into the original
    source.
*/  

#define OPT_TIMED_INPUT

/* OPT_TIMED_INPUT should be defined if your OS allows timed
    input using the halfdelay() curses library call. If this is
    defined, GlkTerm will support timed input. If not, it won't
    (and the -precise command-line option will also be removed.)
   Note that GlkTerm implements time-checking using both the
    halfdelay() call in curses.h, and the gettimeofday() call in
    sys/time.h. If your OS does not support gettimeofday(), you
    will have to comment this option out, unless you want to hack
    gtevent.c to use a different time API.
*/

#define OPT_USE_SIGNALS

/* OPT_USE_SIGNALS should be defined if your OS uses SIGINT, SIGHUP,
    SIGTSTP and SIGCONT signals when the program is interrupted, paused, 
    and resumed. This will likely be true on all Unix systems. (With the
    ctrl-C, ctrl-Z and "fg" commands.)
   If this is defined, GlkTerm will call signal() to set a handler
    for SIGINT and SIGHUP, so that glk_set_interrupt_handler() will work
    right. GlkTerm will also set a handler for SIGCONT, and redraw the
    screen after you resume the program. If this is not defined, GlkTerm
    will not run Glk interrupt handlers, and may not redraw the screen
    until a key is hit.
   The pause/resume (redrawing) functionality will be ignored unless
    OPT_TIMED_INPUT is also defined. This is because GlkTerm has to
    check periodically to see if it's time to redraw the screen. (Not
    the greatest solution, but it works.)
*/

#define OPT_WINCHANGED_SIGNAL

/* OPT_WINCHANGED_SIGNAL should be defined if your OS sends a
    SIGWINCH signal whenever the window size changes. If this
    is defined, GlkTerm will call signal() to set a handler for
    SIGWINCH, and rearrange the screen properly when the window
    is resized. If this is not defined, GlkTerm will think that
    the window size is fixed, and not watch for changes.
   This should generally be defined; comment it out only if your
    OS does not define SIGWINCH.
   OPT_WINCHANGED_SIGNAL will be ignored unless OPT_USE_SIGNALS
    is also defined.
*/

/* #define NO_MEMMOVE */

/* NO_MEMMOVE should be defined if your standard library doesn't
    have a memmove() function. If this is defined, a simple 
    memmove() will be defined in gtmisc.c.
*/

/* Now comes the character localization problem. Since curses.h is
    available, we glibly assume that 7-bit ASCII is all available --
    all the characters from 0x20 (space) to 0x7E (~). Control
    characters should all be available, too, although the library
    interface uses a lot of them. The range 0x7F to 0x9F in Latin-1
    is unprintable. So are only concerned with the range 0xA0 to 0xFF.
*/

/* #define OPT_NATIVE_LATIN_1 */

/* OPT_NATIVE_LATIN_1 should be defined if the character input and
    output on your system uses the Latin-1 character set. In other
    words (for example), if the Glk library prints an 0xE6 character
    (\346), an "ae" ligature must appear on your screen; and if the
    player can type that character, the library must read 0xE6. If
    these things are not true, comment out the OPT_NATIVE_LATIN_1
    definition.
   If this is not defined, Glk does character translation for both
    input and output. It uses OPT_AO_FF_OUTPUT for this purpose; see
    below.
*/

/*
#define OPT_AO_FF_OUTPUT {  \
    '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',  \
    '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',  \
    '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',  \
    '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',  \
    '\300', '\301', '\302', '\303', '\304', '\305', '\306', '\307',  \
    '\310', '\311', '\312', '\313', '\314', '\315', '\316', '\317',  \
    '\320', '\321', '\322', '\323', '\324', '\325', '\326', '\327',  \
    '\330', '\331', '\332', '\333', '\334', '\335', '\336', '\337',  \
    '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',  \
    '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',  \
    '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',  \
    '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',  \
}
*/

/*
#define OPT_AO_FF_OUTPUT {  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
}
*/

#define OPT_AO_FF_OUTPUT {  \
    '\312', '\301', '\242', '\243',  0    , '\264',  0    , '\244',  \
    '\254', '\251', '\273', '\307', '\302', '\320', '\250',  0    ,  \
    '\241', '\261',  0    ,  0    , '\253', '\265', '\246', '\245',  \
     0    ,  0    , '\274', '\310',  0    ,  0    ,  0    , '\300',  \
    '\313',  0    ,  0    , '\314', '\200', '\201', '\256',  0    ,  \
     0    , '\203',  0    ,  0    ,  0    ,  0    ,  0    ,  0    ,  \
     0    , '\204',  0    ,  0    ,  0    , '\315', '\205',  0    ,  \
    '\257',  0    ,  0    ,  0    , '\206',  0    ,  0    , '\247',  \
    '\210', '\207', '\211', '\213', '\212', '\214', '\276', '\215',  \
    '\217', '\216', '\220', '\221', '\223', '\222', '\224', '\225',  \
     0    , '\226', '\230', '\227', '\231', '\233', '\232', '\326',  \
    '\277', '\235', '\234', '\236', '\237',  0    ,  0    , '\330',  \
}

/* OPT_AO_FF_OUTPUT should be defined as a translation table. This is
    ignored if OPT_NATIVE_LATIN_1 is defined. 
   For each value from 0xA0 to 0xFF, define the native character value
    which should be printed for the given Latin-1 character. If your 
    printing system cannot display a particular Latin-1 character, set that
    value to zero. Don't try to make approximations; the library will
    print a sensible ASCII equivalent if you put zero.
   Three samples are given. The first is for a system that can print every
    Latin-1 character. (If you have this, you might as well define
    OPT_NATIVE_LATIN_1; it's a silly sample, but might serve as a starting
    point.) The second sample is for a system which can't print anything
    outside the 0x20 to 0x7E range (7-bit ASCII only.) The third example
    is for the standard Macintosh character set.
*/

#define OPT_AO_FF_TYPABLE {  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
    1, 1, 1, 1, 1, 1, 1, 1,  \
}

/*
#define OPT_AO_FF_TYPABLE {  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
    0, 0, 0, 0, 0, 0, 0, 0,  \
}
*/

/* OPT_AO_FF_TYPABLE should be defined as a table of which characters
    can actually be typed by the player.
   For each value from 0xA0 to 0xFF, give a 1 if the given Latin-1 
    character can be typed, and 0 if not. Don't worry about what native
    character should be translated into that Latin-1 value; the translation
    is done by inverting OPT_AO_FF_OUTPUT.
   OPT_AO_FF_TYPABLE is used whether or not OPT_NATIVE_LATIN_1 is defined.
    This is because the system may be able to display characters that the
    keyboard can't generate.
   However, if OPT_NATIVE_LATIN_1 is off, then any value assigned 0 in
    OPT_AO_FF_OUTPUT is assumed to be 0 in OPT_AO_FF_TYPABLE as well. If
    a character has no exact equivalent on the screen, there won't be an
    exact equivalent on the keyboard -- or so we assume. This means that
    if your keyboard can type every character your screen can display, you
    might as well leave OPT_AO_FF_TYPABLE as all 1's. You only need worry
    about OPT_AO_FF_TYPABLE if there are displayable Latin-1 characters
    which cannot be typed.
   Two samples are given. The first is for a system that can type every
    Latin-1 character. (Or every character that it can display, anyhow.)
    The second is for a keyboard that can only generate 7-bit ASCII.
*/

#endif /* GTOPTION_H */

