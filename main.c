/* main.c: Top-level source file
        for GlkTerm, curses.h implementation of the Glk API.
    GlkTerm Library: version 0.1 alpha.
    Glk API which this implements: version 0.3.
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

/* Declarations of preferences flags. */
int pref_printversion = FALSE;
int pref_screenwidth = 0;
int pref_screenheight = 0;
int pref_messageline = TRUE;
int pref_reverse_textgrids = FALSE;
int pref_window_borders = TRUE;
int pref_precise_timing = FALSE;

/* Some constants for my wacky little command-line option parser. */
#define ex_Void (0)
#define ex_Int (1)
#define ex_Bool (2)

static int errflag = FALSE;

static int extract_value(int argc, char *argv[], char *optname, int type,
    int *argnum, int *result, int defval);
static int string_to_bool(char *str);

int main(int argc, char *argv[])
{
    int ix, val;
    
    /* Test for compile-time errors. If one of these spouts off, you
        must edit glk.h and recompile. */
    if (sizeof(uint32) != 4) {
        printf("Compile-time error: uint32 is not a 32-bit value. Please fix glk.h.\n");
        return 1;
    }
    if ((uint32)(-1) < 0) {
        printf("Compile-time error: uint32 is not unsigned. Please fix glk.h.\n");
        return 1;
    }
    if (sizeof(window_t *) > 4) {
        printf("Compile-time error: Pointers cannot fit in a uint32. Start writing hashtable code.\n");
        return 1;
    }
    
    for (ix=1; ix<argc && !errflag; ix++) {
        if (argv[ix][0] != '-') {
            printf("%s: unwanted argument: %s\n", argv[0], argv[ix]);
            errflag = TRUE;
        }
        else {
            if (extract_value(argc, argv, "?", ex_Void, &ix, &val, FALSE))
                errflag = TRUE;
            else if (extract_value(argc, argv, "help", ex_Void, &ix, &val, FALSE))
                errflag = TRUE;
            else if (extract_value(argc, argv, "version", ex_Void, &ix, &val, FALSE))
                pref_printversion = val;
            else if (extract_value(argc, argv, "v", ex_Void, &ix, &val, FALSE))
                pref_printversion = val;
            else if (extract_value(argc, argv, "width", ex_Int, &ix, &val, 80))
                pref_screenwidth = val;
            else if (extract_value(argc, argv, "w", ex_Int, &ix, &val, 80))
                pref_screenwidth = val;
            else if (extract_value(argc, argv, "height", ex_Int, &ix, &val, 24))
                pref_screenheight = val;
            else if (extract_value(argc, argv, "h", ex_Int, &ix, &val, 24))
                pref_screenheight = val;
            else if (extract_value(argc, argv, "ml", ex_Bool, &ix, &val, pref_messageline))
                pref_messageline = val;
            else if (extract_value(argc, argv, "revgrid", ex_Bool, &ix, &val, pref_reverse_textgrids))
                pref_reverse_textgrids = val;
            else if (extract_value(argc, argv, "border", ex_Bool, &ix, &val, pref_window_borders))
                pref_window_borders = val;
#ifdef OPT_TIMED_INPUT
            else if (extract_value(argc, argv, "precise", ex_Bool, &ix, &val, pref_precise_timing))
                pref_precise_timing = val;
#endif /* !OPT_TIMED_INPUT */
            else {
                printf("%s: unknown option: %s\n", argv[0], argv[ix]);
                errflag = TRUE;
            }
        }
    }
    
    if (errflag) {
        printf("usage: %s [ options ... ]\n", argv[0]);
        printf("options:\n");
        printf("  -width NUM: manual screen width (if not specified, will try to measure)\n");
        printf("  -height NUM: manual screen height (ditto)\n");
        printf("  -ml BOOL: use message line (default 'yes')\n");
        printf("  -revgrid BOOL: reverse text in grid (status) windows (default 'no')\n");
        printf("  -border BOOL: draw borders between windows (default 'yes')\n");
#ifdef OPT_TIMED_INPUT
        printf("  -precise BOOL: more precise timing for timed input (burns more CPU time) (default 'no')\n");
#endif /* !OPT_TIMED_INPUT */
        printf("  -version: display Glk version\n");
        printf("  -help: display this list\n");
        printf("NUM values can be any number. BOOL values can be 'yes' or 'no', or no value to toggle.\n");
        return 1;
    }
    
    if (pref_printversion) {
        printf("GlkTerm, library version %s (%s).\n", 
            LIBRARY_VERSION, LIBRARY_PORT);
        printf("For more information, see http://www.edoc.com/zarf/glk/index.html\n");
        return 1;
    }
    
    /* We now start up curses. From now on, the program must exit through
        glk_exit(), so that endwin() is called. */
    initscr();
    cbreak();
    noecho();
    nonl(); 
    intrflush(stdscr, FALSE); 
    keypad(stdscr, TRUE);
    scrollok(stdscr, FALSE);
    
    /* Initialize things. */
    gli_initialize_misc();
    gli_initialize_windows();
    gli_initialize_events();
    
    /* Call the program main entry point, and then exit. */
    glk_main();
    glk_exit();
    
    /* glk_exit() doesn't return, but the compiler may kvetch if main()
        doesn't seem to return a value. */
    return 0;
}

/* This is my own parsing system for command-line options. It's nothing
    special, but it works. 
   Given argc and argv, check to see if option argnum matches the string
    optname. If so, parse its value according to the type flag. Store the
    result in result if it matches, and return TRUE; return FALSE if it
    doesn't match. argnum is a pointer so that it can be incremented in
    cases like "-width 80". defval is the default value, which is only
    meaningful for boolean options (so that just "-ml" can toggle the
    value of the ml option.) */
static int extract_value(int argc, char *argv[], char *optname, int type,
    int *argnum, int *result, int defval)
{
    int optlen, val;
    char *cx, *origcx, firstch;
    
    optlen = strlen(optname);
    origcx = argv[*argnum];
    cx = origcx;
    
    firstch = *cx;
    cx++;
    
    if (strncmp(cx, optname, optlen))
        return FALSE;
    
    cx += optlen;
    
    switch (type) {
    
        case ex_Void:
            if (*cx)
                return FALSE;
            *result = TRUE;
            return TRUE;
    
        case ex_Int:
            if (*cx == '\0') {
                if ((*argnum)+1 >= argc) {
                    cx = "";
                }
                else {
                    (*argnum) += 1;
                    cx = argv[*argnum];
                }
            }
            val = atoi(cx);
            if (val == 0 && cx[0] != '0') {
                printf("%s: %s must be followed by a number\n", 
                    argv[0], origcx);
                errflag = TRUE;
                return FALSE;
            }
            *result = val;
            return TRUE;

        case ex_Bool:
            if (*cx == '\0') {
                if ((*argnum)+1 >= argc) {
                    val = -1;
                }
                else {
                    char *cx2 = argv[(*argnum)+1];
                    val = string_to_bool(cx2);
                    if (val != -1)
                        (*argnum) += 1;
                }
            }
            else {
                val = string_to_bool(cx);
                if (val == -1) {
                    printf("%s: %s must be followed by a boolean value\n", 
                        argv[0], origcx);
                    errflag = TRUE;
                    return FALSE;
                }
            }
            if (val == -1)
                val = !defval;
            *result = val;
            return TRUE;
            
    }
    
    return FALSE;
}

static int string_to_bool(char *str)
{
    if (!strcmp(str, "y") || !strcmp(str, "yes"))
        return TRUE;
    if (!strcmp(str, "n") || !strcmp(str, "no"))
        return FALSE;
    if (!strcmp(str, "on"))
        return TRUE;
    if (!strcmp(str, "off"))
        return FALSE;
    if (!strcmp(str, "+"))
        return TRUE;
    if (!strcmp(str, "-"))
        return FALSE;
        
    return -1;
}

