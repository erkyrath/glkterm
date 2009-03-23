#include "glk.h"

/* model.c: Model program for Glk API, version 0.3.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
    This program is in the public domain.
*/

/* This is a simple model of a text adventure which uses the Glk API.
    It shows how to input a line of text, display results, maintain a
    status window, write to a transcript file, and so on. */

/* This is the cleanest possible form of a Glk program. It includes only
    "glk.h", and doesn't call any functions outside Glk at all. We even
    define our own str_eq() and str_len(), rather than relying on the
    standard libraries. */

/* We also define our own TRUE and FALSE and NULL. */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

/* The story and status windows. */
static winid_t mainwin = 0;
static winid_t statuswin = 0;

/* A file reference for the transcript file. */
static frefid_t scriptref = 0;
/* A stream for the transcript file, when it's open. */
static strid_t scriptstr = 0;

/* Your location. This determines what appears in the status line. */
static int current_room; 

/* A flag indicating whether you should look around. */
static int need_look; 

/* Forward declarations */
void glk_main(void);

static void draw_statuswin(void);
static int yes_or_no(void);

static int str_eq(char *s1, char *s2);
static int str_len(char *s1);

static void verb_help(void);
static void verb_jump(void);
static void verb_yada(void);
static void verb_move(void);
static void verb_quit(void);
static void verb_script(void);
static void verb_unscript(void);
static void verb_save(void);
static void verb_restore(void);

/* The glk_main() function is called by the Glk system; it's the main entry
    point for your program. */
void glk_main(void)
{
    /* Open the main window. */
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    if (!mainwin) {
        /* It's possible that the main window failed to open. There's
            nothing we can do without it, so exit. */
        return; 
    }
    
    /* Set the current output stream to print to it. */
    glk_set_window(mainwin);
    
    /* Open a second window: a text grid, above the main window, three lines
        high. It is possible that this will fail also, but we accept that. */
    statuswin = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed, 
        3, wintype_TextGrid, 0);

    glk_put_string("Model Glk Program\nAn Interactive Model Glk Program\n");
    glk_put_string("By Andrew Plotkin.\nRelease 4.\n");
    glk_put_string("Type \"help\" for a list of commands.\n");
    
    current_room = 0; /* Set initial location. */
    need_look = TRUE;
    
    while (1) {
        char commandbuf[256];
        char *cx, *cmd;
        int gotline, len;
        event_t ev;
        
        draw_statuswin();
        
        if (need_look) {
            need_look = FALSE;
            glk_put_string("\n");
            glk_set_style(style_Subheader);
            if (current_room == 0)
                glk_put_string("The Room\n");
            else
                glk_put_string("A Different Room\n");
            glk_set_style(style_Normal);
            glk_put_string("You're in a room of some sort.\n");
        }
        
        glk_put_string("\n>");
        /* We request up to 255 characters. The buffer can hold 256, but we
            are going to stick a null character at the end, so we have to
            leave room for that. Note that the Glk library does *not*
            put on that null character. */
        glk_request_line_event(mainwin, commandbuf, 255, 0);
        
        gotline = FALSE;
        while (!gotline) {
        
            /* Grab an event. */
            glk_select(&ev);
            
            switch (ev.type) {
            
                case evtype_LineInput:
                    if (ev.win == mainwin) {
                        gotline = TRUE;
                        /* Really the event can *only* be from mainwin,
                            because we never request line input from the
                            status window. But we do a paranoia test,
                            because commandbuf is only filled if the line
                            event comes from the mainwin request. If the
                            line event comes from anywhere else, we ignore
                            it. */
                    }
                    break;
                    
                case evtype_Arrange:
                    /* Windows have changed size, so we have to redraw the
                        status window. */
                    draw_statuswin();
                    break;
            }
        }
        
        /* commandbuf now contains a line of input from the main window.
            You would now run your parser and do something with it. */
        
        /* The line we have received in commandbuf is not null-terminated.
            We handle that first. */
        len = ev.val1; /* Will be between 0 and 255, inclusive. */
        commandbuf[len] = '\0';
        
        /* Then squash to lower-case. */
        for (cx = commandbuf; *cx; cx++) { 
            *cx = glk_char_to_lower(*cx);
        }
        
        /* Then trim whitespace before and after. */
        
        for (cx = commandbuf; *cx == ' '; cx++) { };
        
        cmd = cx;
        
        for (cx = commandbuf+len-1; cx >= cmd && *cx == ' '; cx--) { };
        *(cx+1) = '\0';
        
        /* cmd now points to a nice null-terminated string. We'll do the
            simplest possible parsing. */
        if (str_eq(cmd, "")) {
            glk_put_string("Excuse me?\n");
        }
        else if (str_eq(cmd, "help")) {
            verb_help();
        }
        else if (str_eq(cmd, "move")) {
            verb_move();
        }
        else if (str_eq(cmd, "jump")) {
            verb_jump();
        }
        else if (str_eq(cmd, "yada")) {
            verb_yada();
        }
        else if (str_eq(cmd, "quit")) {
            verb_quit();
        }
        else if (str_eq(cmd, "save")) {
            verb_save();
        }
        else if (str_eq(cmd, "restore")) {
            verb_restore();
        }
        else if (str_eq(cmd, "script")) {
            verb_script();
        }
        else if (str_eq(cmd, "unscript")) {
            verb_unscript();
        }
        else {
            glk_put_string("I don't understand the command \"");
            glk_put_string(cmd);
            glk_put_string("\".\n");
        }
    }
}

static void draw_statuswin(void)
{
    char *roomname;
    uint32 width, height;
    
    if (!statuswin) {
        /* It is possible that the window was not successfully 
            created. If that's the case, don't try to draw it. */
        return;
    }
    
    if (current_room == 0)
        roomname = "The Room";
    else
        roomname = "A Different Room";
    
    glk_set_window(statuswin);
    glk_window_clear(statuswin);
    
    glk_window_get_size(statuswin, &width, &height);
    
    /* Print the room name, centered. */
    glk_window_move_cursor(statuswin, (width - str_len(roomname)) / 2, 1);
    glk_put_string(roomname);
    
    /* Draw a decorative compass rose in the upper right. */
    glk_window_move_cursor(statuswin, width - 3, 0);
    glk_put_string("\\|/");
    glk_window_move_cursor(statuswin, width - 3, 1);
    glk_put_string("-*-");
    glk_window_move_cursor(statuswin, width - 3, 2);
    glk_put_string("/|\\");
    
    glk_set_window(mainwin);
}

static int yes_or_no(void)
{
    char commandbuf[256];
    char *cx, *cmd;
    int gotline, len;
    event_t ev;
    
    draw_statuswin();
    
    /* This loop is identical to the main command loop in glk_main(). */
    
    while (1) {
        glk_request_line_event(mainwin, commandbuf, 255, 0);
        
        gotline = FALSE;
        while (!gotline) {
        
            glk_select(&ev);
            
            switch (ev.type) {
                case evtype_LineInput:
                    if (ev.win == mainwin) {
                        gotline = TRUE;
                    }
                    break;
                    
                case evtype_Arrange:
                    draw_statuswin();
                    break;
            }
        }
        
        len = ev.val1;
        commandbuf[len] = '\0';
        for (cx = commandbuf; *cx == ' '; cx++) { };
        
        if (*cx == 'y' || *cx == 'Y')
            return TRUE;
        if (*cx == 'n' || *cx == 'N')
            return FALSE;
            
        glk_put_string("Please enter \"yes\" or \"no\": ");
    }
    
}

static void verb_help(void)
{
    glk_put_string("This model only understands the following commands:\n");
    glk_put_string("HELP: Display this list.\n");
    glk_put_string("JUMP: A verb which just prints some text.\n");
    glk_put_string("YADA: A verb which prints a very long stream of text.\n");
    glk_put_string("MOVE: A verb which prints some text, and also changes the status line display.\n");
    glk_put_string("SCRIPT: Turn on transcripting, so that output will be echoed to a text file.\n");
    glk_put_string("UNSCRIPT: Turn off transcripting.\n");
    glk_put_string("SAVE: Write fake data to a save file.\n");
    glk_put_string("RESTORE: Read it back in.\n");
    glk_put_string("QUIT: Quit and exit.\n");
}

static void verb_jump(void)
{
    glk_put_string("You jump on the fruit, spotlessly.\n");
}

static void verb_yada(void)
{
    /* This is a goofy (and overly ornate) way to print a long paragraph. 
        It just shows off line wrapping in the Glk implementation. */
    #define NUMWORDS (13)
    static char *wordcaplist[NUMWORDS] = {
        "Ga", "Bo", "Wa", "Mu", "Bi", "Fo", "Za", "Mo", "Ra", "Po",
            "Ha", "Ni", "Na"
    };
    static char *wordlist[NUMWORDS] = {
        "figgle", "wob", "shim", "fleb", "moobosh", "fonk", "wabble",
            "gazoon", "ting", "floo", "zonk", "loof", "lob",
    };
    static int wcount1 = 0;
    static int wcount2 = 0;
    static int wstep = 1;
    static int jx = 0;
    int ix;
    int first = TRUE;
    
    for (ix=0; ix<85; ix++) {
        if (ix > 0) {
            glk_put_string(" ");
        }
                
        if (first) {
            glk_put_string(wordcaplist[(ix / 17) % NUMWORDS]);
            first = FALSE;
        }
        
        glk_put_string(wordlist[jx]);
        jx = (jx + wstep) % NUMWORDS;
        wcount1++;
        if (wcount1 >= NUMWORDS) {
            wcount1 = 0;
            wstep++;
            wcount2++;
            if (wcount2 >= NUMWORDS-2) {
                wcount2 = 0;
                wstep = 1;
            }
        }
        
        if ((ix % 17) == 16) {
            glk_put_string(".");
            first = TRUE;
        }
    }
    
    glk_put_char('\n');
}

static void verb_move(void)
{
    current_room = (current_room+1) % 2;
    need_look = TRUE;
    
    glk_put_string("You walk for a while.\n");
}

static void verb_quit(void)
{
    glk_put_string("Are you sure you want to quit? ");
    if (yes_or_no()) {
        glk_put_string("Thanks for playing.\n");
        glk_exit();
        /* glk_exit() actually stops the process; it does not return. */
    }
}

static void verb_script(void)
{
    if (scriptstr) {
        glk_put_string("Scripting is already on.\n");
        return;
    }
    
    /* If we've turned on scripting before, use the same file reference; 
        otherwise, prompt the player for a file. */
    if (!scriptref) {
        scriptref = glk_fileref_create_by_prompt(
            fileusage_Transcript | fileusage_TextMode, 
            filemode_WriteAppend, 0);
        if (!scriptref) {
            glk_put_string("Unable to place script file.\n");
            return;
        }
    }
    
    /* Open the file. */
    scriptstr = glk_stream_open_file(scriptref, filemode_WriteAppend, 0);
    if (!scriptstr) {
        glk_put_string("Unable to write to script file.\n");
        return;
    }
    glk_put_string("Scripting on.\n");
    glk_window_set_echo_stream(mainwin, scriptstr);
    glk_put_string_stream(scriptstr, 
        "This is the beginning of a transcript.\n");
}

static void verb_unscript(void)
{
    if (!scriptstr) {
        glk_put_string("Scripting is already off.\n");
        return;
    }
    
    /* Close the file. */
    glk_put_string_stream(scriptstr, 
        "This is the end of a transcript.\n\n");
    glk_stream_close(scriptstr, NULL);
    glk_put_string("Scripting off.\n");
    scriptstr = 0;
}

static void verb_save(void)
{
    int ix;
    frefid_t saveref;
    strid_t savestr;
    
    saveref = glk_fileref_create_by_prompt(
        fileusage_SavedGame | fileusage_BinaryMode, 
        filemode_Write, 0);
    if (!saveref) {
        glk_put_string("Unable to place save file.\n");
        return;
    }
    
    savestr = glk_stream_open_file(saveref, filemode_Write, 0);
    if (!savestr) {
        glk_put_string("Unable to write to save file.\n");
        glk_fileref_destroy(saveref);
        return;
    }

    glk_fileref_destroy(saveref); /* We're done with the file ref now. */
    
    /* Write some binary data. */
    for (ix=0; ix<256; ix++) {
        glk_put_char_stream(savestr, ix);
    }
    
    glk_stream_close(savestr, NULL);
    
    glk_put_string("Game saved.\n");
}

static void verb_restore(void)
{
    int ix;
    int err;
    uint32 ch;
    frefid_t saveref;
    strid_t savestr;
    
    saveref = glk_fileref_create_by_prompt(
        fileusage_SavedGame | fileusage_BinaryMode, 
        filemode_Read, 0);
    if (!saveref) {
        glk_put_string("Unable to find save file.\n");
        return;
    }
    
    savestr = glk_stream_open_file(saveref, filemode_Read, 0);
    if (!savestr) {
        glk_put_string("Unable to read from save file.\n");
        glk_fileref_destroy(saveref);
        return;
    }

    glk_fileref_destroy(saveref); /* We're done with the file ref now. */
    
    /* Read some binary data. */
    err = FALSE;
    
    for (ix=0; ix<256; ix++) {
        ch = glk_get_char_stream(savestr);
        if (ch == -1) {
            glk_put_string("Unexpected end of file.\n");
            err = TRUE;
            break;
        }
        if (ch != ix) {
            glk_put_string("This does not appear to be a valid saved game.\n");
            err = TRUE;
            break;
        }
    }
    
    glk_stream_close(savestr, NULL);
    
    if (err) {
        glk_put_string("Failed.\n");
        return;
    }
    
    glk_put_string("Game restored.\n");
}

/* simple string length test */
static int str_len(char *s1)
{
    int len;
    for (len = 0; *s1; s1++)
        len++;
    return len;
}

/* simple string comparison test */
static int str_eq(char *s1, char *s2)
{
    for (; *s1 && *s2; s1++, s2++) {
        if (*s1 != *s2)
            return FALSE;
    }
    
    if (*s1 || *s2)
        return FALSE;
    else
        return TRUE;
}

