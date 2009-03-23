/* glkterm.h: Private header file
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
*/

#ifndef GLKTERM_H
#define GLKTERM_H

/* We define our own TRUE and FALSE and NULL, because ANSI
    is a strange world. */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

/* Now, some macros to convert object IDs to internal object pointers.
    I am doing this in the cheapest possible way: I cast the pointer to
    an integer. This assumes that glui32 is big enough to hold an 
    arbitrary pointer. (If this is not true, don't even try to run this 
    code.)
   The first field of each object is a magic number which identifies the
    object type. This lets us have a *little* bit of run-time type 
    safety; if you try to convert an ID of the wrong type, the macro 
    will return NULL. But this is not very safe, because a random ID 
    value can still cause a memory fault or crash.
   A safer approach would be to store IDs in a hash table, and rewrite
    these macros to perform hash table lookups and references. Feel 
    free.
   Note that these macros are not intended to convert between the values
    NULL and 0. (It is true that WindowToID(NULL) will evaluate to 0 on
    almost all systems, but the library code does not rely on this, so
    if you rewrite WindowToID you don't need to guarantee it.) */

#define WindowToID(win)  ((glui32)(win))
#define StreamToID(str)  ((glui32)(str))
#define FilerefToID(fref)  ((glui32)(fref))

#define IDToWindow(id)    \
    ((((window_t *)(id))->magicnum == MAGIC_WINDOW_NUM)    \
    ? ((window_t *)(id)) : NULL)
#define IDToStream(id)    \
    ((((stream_t *)(id))->magicnum == MAGIC_STREAM_NUM)    \
    ? ((stream_t *)(id)) : NULL)
#define IDToFileref(id)    \
    ((((fileref_t *)(id))->magicnum == MAGIC_FILEREF_NUM)    \
    ? ((fileref_t *)(id)) : NULL)

/* This macro is called whenever the library code catches an error
    or illegal operation from the game program. */

#define gli_strict_warning(msg)   \
    (gli_msgline_warning(msg)) 

/* Some useful type declarations. */

typedef struct grect_struct {
    int left, top;
    int right, bottom;
} grect_t;

typedef struct window_struct window_t;
typedef struct stream_struct stream_t;
typedef struct fileref_struct fileref_t;

#define MAGIC_WINDOW_NUM (9826)
#define MAGIC_STREAM_NUM (8269)
#define MAGIC_FILEREF_NUM (6982)

struct window_struct {
    glui32 magicnum;
    glui32 rock;
    glui32 type;
    
    grect_t bbox; /* content rectangle, excluding borders */
    window_t *parent; /* pair window which contains this one */
    void *data; /* one of the window_*_t structures */
    
    stream_t *str; /* the window stream. */
    stream_t *echostr; /* the window's echo stream, if any. */
    
    int line_request;
    int char_request;

    glui32 style;
    
    window_t *next; /* in the big linked list of windows */
};

#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)

struct stream_struct {
    glui32 magicnum;
    glui32 rock;

    int type; /* file, window, or memory stream */
    
    glui32 readcount, writecount;
    int readable, writable;
    
    /* for strtype_Window */
    window_t *win;
    
    /* for strtype_File */
    FILE *file; 
    
    /* for strtype_Memory */
    unsigned char *buf;
    unsigned char *bufptr;
    unsigned char *bufend;
    unsigned char *bufeof;
    glui32 buflen;

    stream_t *next; /* in the big linked list of streams */
};

struct fileref_struct {
    glui32 magicnum;
    glui32 rock;

    char *filename;
    int filetype;
    int textmode;

    fileref_t *next; /* in the big linked list of filerefs */
};

/* Arguments to keybindings */

#define gcmd_Left (1)
#define gcmd_Right (2)
#define gcmd_Up (3)
#define gcmd_Down (4)
#define gcmd_LeftEnd (5)
#define gcmd_RightEnd (6)
#define gcmd_UpEnd (7)
#define gcmd_DownEnd (8)
#define gcmd_UpPage (9)
#define gcmd_DownPage (10)
#define gcmd_Delete (11)
#define gcmd_DeleteNext (12)
#define gcmd_KillInput (13)
#define gcmd_KillLine (14)

/* A few global variables */

extern window_t *gli_rootwin;
extern window_t *gli_focuswin;
extern grect_t content_box;
extern void (*gli_interrupt_handler)(void);

#ifdef OPT_USE_SIGNALS
    extern int just_resumed;
    extern int just_killed;
#ifdef OPT_WINCHANGED_SIGNAL
        extern int screen_size_changed;
#endif /* OPT_WINCHANGED_SIGNAL */
#endif /* OPT_USE_SIGNALS */

extern unsigned char char_printable_table[256];
extern unsigned char char_typable_table[256];
#ifndef OPT_NATIVE_LATIN_1
extern unsigned char char_from_native_table[256];
extern unsigned char char_to_native_table[256];
#endif /* OPT_NATIVE_LATIN_1 */

extern int pref_printversion;
extern int pref_screenwidth;
extern int pref_screenheight;
extern int pref_messageline;
extern int pref_reverse_textgrids;
extern int pref_window_borders;
extern int pref_precise_timing;

/* Declarations of library internal functions. */

extern void gli_initialize_misc(void);
extern char *gli_ascii_equivalent(unsigned char ch);

extern void gli_msgline_warning(char *msg);
extern void gli_msgline(char *msg);
extern void gli_msgline_redraw(void);

extern int gli_msgin_getline(char *prompt, char *buf, int maxlen, int *length);
extern int gli_msgin_getchar(char *prompt, int hilite);

extern void gli_initialize_events(void);
extern void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2);
extern void gli_set_halfdelay(void);

extern void gli_input_handle_key(int key);
extern void gli_input_guess_focus(void);

extern void gli_initialize_windows(void);
extern void gli_setup_curses(void);
extern void gli_fast_exit(void);
extern window_t *gli_new_window(glui32 type, glui32 rock);
extern void gli_delete_window(window_t *win);
extern window_t *gli_window_iterate_treeorder(window_t *win);
extern void gli_window_rearrange(window_t *win, grect_t *box);
extern void gli_window_redraw(window_t *win);
extern void gli_windows_redraw(void);
extern void gli_windows_update(void);
extern void gli_windows_size_change(void);
extern void gli_windows_place_cursor(void);
extern void gli_windows_set_paging(int forcetoend);
extern void gli_windows_trim_buffers(void);
extern void gli_window_put_char(window_t *win, char ch);
extern void gli_windows_unechostream(stream_t *str);
extern void gli_print_spaces(int len);

extern void gcmd_win_change_focus(window_t *win, glui32 arg);
extern void gcmd_win_refresh(window_t *win, glui32 arg);

extern stream_t *gli_new_stream(int type, int readable, int writable, 
    glui32 rock);
extern void gli_delete_stream(stream_t *str);
extern stream_t *gli_stream_open_window(window_t *win);
extern void gli_stream_set_current(stream_t *str);
extern void gli_stream_fill_result(stream_t *str, 
    stream_result_t *result);
extern void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
extern void gli_streams_close_all(void);

extern fileref_t *gli_new_fileref(char *filename, glui32 usage, 
    glui32 rock);
extern void gli_delete_fileref(fileref_t *fref);

/* A macro that I can't think of anywhere else to put it. */

#define gli_event_clearevent(evp)  \
    ((evp)->type = evtype_None,    \
    (evp)->win = 0,    \
    (evp)->val1 = 0,   \
    (evp)->val2 = 0)

#ifdef NO_MEMMOVE
    extern void *memmove(void *dest, void *src, int n);
#endif /* NO_MEMMOVE */

#endif /* GLKTERM_H */
