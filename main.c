/* main.c: Top-level source file
        for GlkTerm, curses.h implementation of the Glk API.
    Glk API which this implements: version 0.7.1.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://eblong.com/zarf/glk/
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "glk.h"
#include "glkterm.h"
#include "glkstart.h"

/* Declarations of preferences flags. */
int pref_printversion = FALSE;
int pref_screenwidth = 0;
int pref_screenheight = 0;
int pref_messageline = TRUE;
int pref_reverse_textgrids = FALSE;
int pref_override_window_borders = FALSE;
int pref_window_borders = FALSE;
int pref_precise_timing = FALSE;
int pref_historylen = 20;
int pref_prompt_defaults = TRUE;

/* Some constants for my wacky little command-line option parser. */
#define ex_Void (0)
#define ex_Int (1)
#define ex_Bool (2)
#define ex_String (3)

static int errflag = FALSE;
static int inittime = FALSE;

static int extract_value(int argc, char *argv[], char *optname, int type,
    int *argnum, int *result, int defval);
static int extract_value_string(int argc, char *argv[], char *optname, int type,
    int *argnum, const char **result);
static int string_to_bool(char *str);
static int parse_style_file(const char *filename);

/* glk style names in order of constant */
static const char *style_names[] = {
    "Normal",
    "Emphasized",
    "Preformatted",
    "Header",
    "Subheader",
    "Alert",
    "Note",
    "BlockQuote",
    "Input",
    "User1",
    "User2",
    NULL
};

/* curses attributes by name */
static const struct {
    const char *name;
    int bit;
} style_attributes[] = {
    {"STANDOUT", A_STANDOUT},
    {"UNDERLINE", A_UNDERLINE},
    {"REVERSE", A_REVERSE},
    {"BLINK", A_BLINK},
    {"DIM", A_DIM},
    {"BOLD", A_BOLD},
    {"ALTCHARSET", A_ALTCHARSET},
    {"INVIS", A_INVIS},
    {"PROTECT", A_PROTECT},
    {NULL}
};

int main(int argc, char *argv[])
{
    int ix, jx, val;
    glkunix_startup_t startdata;
    
    /* Test for compile-time errors. If one of these spouts off, you
        must edit glk.h and recompile. */
    if (sizeof(glui32) != 4) {
        printf("Compile-time error: glui32 is not a 32-bit value. Please fix glk.h.\n");
        return 1;
    }
    if ((glui32)(-1) < 0) {
        printf("Compile-time error: glui32 is not unsigned. Please fix glk.h.\n");
        return 1;
    }

    /* Now some argument-parsing. This is probably going to hurt. */
    startdata.argc = 0;
    startdata.argv = (char **)malloc(argc * sizeof(char *));
    
    /* Copy in the program name. */
    startdata.argv[startdata.argc] = argv[0];
    startdata.argc++;
    
    for (ix=1; ix<argc && !errflag; ix++) {
        glkunix_argumentlist_t *argform;
        int inarglist = FALSE;
        char *cx;
        
        for (argform = glkunix_arguments; 
            argform->argtype != glkunix_arg_End && !errflag; 
            argform++) {
            
            if (argform->name[0] == '\0') {
                if (argv[ix][0] != '-') {
                    startdata.argv[startdata.argc] = argv[ix];
                    startdata.argc++;
                    inarglist = TRUE;
                }
            }
            else if ((argform->argtype == glkunix_arg_NumberValue)
                && !strncmp(argv[ix], argform->name, strlen(argform->name))
                && (cx = argv[ix] + strlen(argform->name))
                && (atoi(cx) != 0 || cx[0] == '0')) {
                startdata.argv[startdata.argc] = argv[ix];
                startdata.argc++;
                inarglist = TRUE;
            }
            else if (!strcmp(argv[ix], argform->name)) {
                int numeat = 0;
                
                if (argform->argtype == glkunix_arg_ValueFollows) {
                    if (ix+1 >= argc) {
                        printf("%s: %s must be followed by a value\n", 
                            argv[0], argform->name);
                        errflag = TRUE;
                        break;
                    }
                    numeat = 2;
                }
                else if (argform->argtype == glkunix_arg_NoValue) {
                    numeat = 1;
                }
                else if (argform->argtype == glkunix_arg_ValueCanFollow) {
                    if (ix+1 < argc && argv[ix+1][0] != '-') {
                        numeat = 2;
                    }
                    else {
                        numeat = 1;
                    }
                }
                else if (argform->argtype == glkunix_arg_NumberValue) {
                    if (ix+1 >= argc
                        || (atoi(argv[ix+1]) == 0 && argv[ix+1][0] != '0')) {
                        printf("%s: %s must be followed by a number\n", 
                            argv[0], argform->name);
                        errflag = TRUE;
                        break;
                    }
                    numeat = 2;
                }
                else {
                    errflag = TRUE;
                    break;
                }
                
                for (jx=0; jx<numeat; jx++) {
                    startdata.argv[startdata.argc] = argv[ix];
                    startdata.argc++;
                    if (jx+1 < numeat)
                        ix++;
                }
                inarglist = TRUE;
                break;
            }
        }
        if (inarglist || errflag)
            continue;
            
        if (argv[ix][0] != '-') {
            printf("%s: unwanted argument: %s\n", argv[0], argv[ix]);
            errflag = TRUE;
            break;
        }
        const char *strval;
        if (extract_value(argc, argv, "?", ex_Void, &ix, &val, FALSE))
            errflag = TRUE;
        else if (extract_value(argc, argv, "help", ex_Void, &ix, &val, FALSE))
            errflag = TRUE;
        else if (extract_value(argc, argv, "version", ex_Void, &ix, &val, FALSE))
            pref_printversion = val;
        else if (extract_value(argc, argv, "v", ex_Void, &ix, &val, FALSE))
            pref_printversion = val;
        else if (extract_value(argc, argv, "historylen", ex_Int, &ix, &val, 20))
            pref_historylen = val;
        else if (extract_value(argc, argv, "hl", ex_Int, &ix, &val, 20))
            pref_historylen = val;
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
        else if (extract_value(argc, argv, "border", ex_Bool, &ix, &val, pref_window_borders)) {
            pref_window_borders = val;
            pref_override_window_borders = TRUE;
        }
        else if (extract_value(argc, argv, "defprompt", ex_Bool, &ix, &val, pref_prompt_defaults))
            pref_prompt_defaults = val;
#ifdef OPT_TIMED_INPUT
        else if (extract_value(argc, argv, "precise", ex_Bool, &ix, &val, pref_precise_timing))
            pref_precise_timing = val;
#endif /* !OPT_TIMED_INPUT */
        else if (extract_value_string(argc, argv, "style", ex_String, &ix, &strval)) {
            if (!parse_style_file(strval))
                return 1;
        }
        else {
            printf("%s: unknown option: %s\n", argv[0], argv[ix]);
            errflag = TRUE;
        }
    }
    
    if (errflag) {
        printf("usage: %s [ options ... ]\n", argv[0]);
        if (glkunix_arguments[0].argtype != glkunix_arg_End) {
            glkunix_argumentlist_t *argform;
            printf("game options:\n");
            for (argform = glkunix_arguments; 
                argform->argtype != glkunix_arg_End; 
                argform++) {
                if (strlen(argform->name) == 0)
                    printf("  %s\n", argform->desc);
                else if (argform->argtype == glkunix_arg_ValueFollows)
                    printf("  %s val: %s\n", argform->name, argform->desc);
                else if (argform->argtype == glkunix_arg_NumberValue)
                    printf("  %s val: %s\n", argform->name, argform->desc);
                else if (argform->argtype == glkunix_arg_ValueCanFollow)
                    printf("  %s [val]: %s\n", argform->name, argform->desc);
                else
                    printf("  %s: %s\n", argform->name, argform->desc);
            }
        }
        printf("library options:\n");
        printf("  -width NUM: manual screen width (if not specified, will try to measure)\n");
        printf("  -height NUM: manual screen height (ditto)\n");
        printf("  -ml BOOL: use message line (default 'yes')\n");
        printf("  -historylen NUM: length of command history (default 20)\n");
        printf("  -revgrid BOOL: reverse text in grid (status) windows (default 'no')\n");
        printf("  -border BOOL: force borders/no borders between windows\n");
        printf("  -defprompt BOOL: provide defaults for file prompts (default 'yes')\n");
#ifdef OPT_TIMED_INPUT
        printf("  -precise BOOL: more precise timing for timed input (burns more CPU time) (default 'no')\n");
#endif /* !OPT_TIMED_INPUT */
        printf("  -style FILENAME: read visual styles from provided file\n");
        printf("  -version: display Glk library version\n");
        printf("  -help: display this list\n");
        printf("NUM values can be any number. BOOL values can be 'yes' or 'no', or no value to toggle.\n");
        return 1;
    }
    
    if (pref_printversion) {
        printf("GlkTerm, library version %s (%s).\n", 
            LIBRARY_VERSION, LIBRARY_PORT);
        printf("For more information, see http://eblong.com/zarf/glk/\n");
        return 1;
    }
    
    /* We now start up curses. From now on, the program must exit through
        glk_exit(), so that endwin() is called. */
    gli_setup_curses();
    
    /* Initialize things. */
    gli_initialize_misc();
    gli_initialize_windows();
    gli_initialize_events();
    
    inittime = TRUE;
    if (!glkunix_startup_code(&startdata)) {
        glk_exit();
    }
    inittime = FALSE;
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

/* extract_value call for strings arguments.
 * The 'type' argument must always be ex_String.
 * returns a pointer to the argument in-place. This pointer is only valid as long as the
 * argv pointer is valid, and does not need to be freed.
 */
static int extract_value_string(int argc, char *argv[], char *optname, int type,
    int *argnum, const char **result)
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
        case ex_String:
            if (*cx == '\0') {
                if ((*argnum)+1 >= argc) {
                    return FALSE;
                }
                else {
                    *result = argv[(*argnum)+1];
                    *argnum += 1;
                    return TRUE;
                }
            }
            else {
                *result = cx;
                return TRUE;
            }
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

/* This opens a file for reading or writing. (You cannot open a file
   for appending using this call.)

   This should be used only by glkunix_startup_code(). 
*/
strid_t glkunix_stream_open_pathname_gen(char *pathname, glui32 writemode,
    glui32 textmode, glui32 rock)
{
    if (!inittime)
        return 0;
    return gli_stream_open_pathname(pathname, (writemode != 0), (textmode != 0), rock);
}

/* This opens a file for reading. It is a less-general form of 
   glkunix_stream_open_pathname_gen(), preserved for backwards 
   compatibility.

   This should be used only by glkunix_startup_code().
*/
strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, 
    glui32 rock)
{
    if (!inittime)
        return 0;
    return gli_stream_open_pathname(pathname, FALSE, (textmode != 0), rock);
}

/* Parse 'Style = fg,bg,attrs' tuple */
static int parse_style_key_value(int linenr, const char *key, char *value, int styles[style_NUMSTYLES][3])
{
    int style_id = 0;

    for (style_id = 0; style_names[style_id]; ++style_id)
        if (!strcmp(style_names[style_id], key))
            break;
    if (!style_names[style_id])
    {
        printf("Unknown style id %s on line %i\n", key, linenr);
        return 0;
    }

    char *fg_str = value;
    char *bg_str = strchr(fg_str, ',');
    if(bg_str == NULL)
    {
        printf("Missing background color on line %i\n", linenr);
        return 0;
    }
    *bg_str = 0; /* terminate with zero */
    bg_str++;
    char *attrs_str = strchr(bg_str, ',');
    if(attrs_str == NULL)
    {
        printf("Missing attributes on line %i\n", linenr);
        return 0;
    }
    *attrs_str = 0; /* terminate with zero and advance */
    attrs_str++;

    /* parse attributes list */
    int attrs = 0;
    char *token;
    while (token = strtok(attrs_str, " "))
    {
        int attr_id;
        for (attr_id = 0; style_attributes[attr_id].name; ++attr_id)
            if (!strcmp(style_attributes[attr_id].name, token))
                break;
        if (!style_attributes[attr_id].name)
        {
            printf("Unknown attribute %s on line %i\n", token, linenr);
            return 0;
        }
        attrs |= style_attributes[attr_id].bit;
        attrs_str = NULL;
    }

    styles[style_id][0] = atoi(fg_str);
    styles[style_id][1] = atoi(bg_str);
    styles[style_id][2] = attrs;
    return TRUE;
}

extern int win_textgrid_styles[style_NUMSTYLES][3];
extern int win_textbuffer_styles[style_NUMSTYLES][3];
extern int win_separator_styles[style_NUMSTYLES][3];
#define LINESIZE 1000
/* Simple parser for 'curses style sheet'. An example:

    [textbuffer]
    Normal        = -1, -1,
    Emphasized    = -1, -1, UNDERLINE
    Preformatted  = -1, -1,
    Header        = -1, -1, BOLD
    Subheader     = -1, -1, BOLD
    Alert         = -1, -1, REVERSE
    Note          = -1, -1, UNDERLINE
    BlockQuote    = -1, -1,
    Input         = -1, -1, BOLD
    User1         = -1, -1,
    User2         = -1, -1,

    [textgrid]
    ...
 */
static int parse_style_file(const char *filename)
{
    FILE *f;
    char line[LINESIZE];
    int linenr = 1;
    int (*styles)[3] = NULL;

    f = fopen(filename, "r");
    if (f == NULL)
    {
        printf("Could not open style file %s\n", filename);
        return FALSE;
    }
    while (fgets(line, LINESIZE, f))
    {
        char *ptr = line;
        /* '#' begins a comment - ignore comments */
        char *eol = strchr(line, '#');
        if(!eol) /* fgets leaves the newline */
            eol = strchr(line, '\n');
        if(!eol)
            eol = line + strlen(line);
        *eol = 0;

        /* skip spaces at beginning of line */
        while (*ptr && isspace(*ptr))
            ptr++;

        if (*ptr == '[') /* [textbuffer] */
        {
            ptr++;
            const char *cat_name = &*ptr;
            while (*ptr && *ptr != ']')
                ptr++;
            if (*ptr != ']')
            {
                printf("Unterminated category name on line %i\n", linenr);
                return FALSE;
            }
            *ptr = 0;
            ptr++;
            if (!strcmp(cat_name, "textbuffer"))
            {
                styles = win_textbuffer_styles;
            } else if (!strcmp(cat_name, "textgrid"))
            {
                styles = win_textgrid_styles;
            } else if (!strcmp(cat_name, "separator"))
            {
                styles = win_separator_styles;
            } else
            {
                printf("Unknown category name on line %i: %s\n", linenr, cat_name);
                return FALSE;
            }
        } else if (*ptr) {
            /* non-empty line: key = value */
            if(!styles)
            {
                printf("Assignment without category on line %i\n", linenr);
            }
            const char *key = &*ptr;
            while (*ptr && !isspace(*ptr))
                ptr++;
            char *end_of_key = &*ptr;
            while (*ptr && isspace(*ptr))
                ptr++;
            if(!*ptr || *ptr != '=')
            {
                printf("Invalid or missing assignment on line %i\n", linenr);
                return FALSE;
            }
            *end_of_key = 0; /* make sure that key is zero-terminated */
            ptr += 1;
            while (*ptr && isspace(*ptr)) /* skip spaces after '=' */
                ptr += 1;
            if(!parse_style_key_value(linenr, key, &*ptr, styles))
                return FALSE;
            ptr = eol;
        }
        while (*ptr && isspace(*ptr))
            ptr++;
        if (*ptr)
        {
            printf("Garbage at end of line %i: %s\n", linenr, &*ptr);
            return FALSE;
        }
        linenr += 1;
    }
    fclose(f);
    return TRUE;
}


