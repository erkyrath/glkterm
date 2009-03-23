/* gtfref.c: File reference objects
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* for unlink() */
#include <sys/stat.h> /* for stat() */
#include "glk.h"
#include "glkterm.h"

/* This code implements filerefs as they work in a stdio system: a
    fileref contains a pathname, a text/binary flag, and a file
    type.
*/

/* Linked list of all filerefs */
static fileref_t *gli_filereflist = NULL; 

fileref_t *gli_new_fileref(char *filename, glui32 usage, glui32 rock)
{
    fileref_t *fref = (fileref_t *)malloc(sizeof(fileref_t));
    if (!fref)
        return NULL;
    
    fref->magicnum = MAGIC_FILEREF_NUM;
    fref->rock = rock;
    
    fref->filename = malloc(1 + strlen(filename));
    strcpy(fref->filename, filename);
    
    fref->textmode = ((usage & fileusage_TextMode) != 0);
    fref->filetype = (usage & fileusage_TypeMask);
    
    fref->prev = NULL;
    fref->next = gli_filereflist;
    gli_filereflist = fref;
    if (fref->next) {
        fref->next->prev = fref;
    }
    
    if (gli_register_obj)
        fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);

    return fref;
}

void gli_delete_fileref(fileref_t *fref)
{
    fileref_t *prev, *next;
    
    if (gli_unregister_obj)
        (*gli_unregister_obj)(fref, gidisp_Class_Fileref, fref->disprock);
        
    fref->magicnum = 0;
    
    if (fref->filename) {
        free(fref->filename);
        fref->filename = NULL;
    }
    
    prev = fref->prev;
    next = fref->next;
    fref->prev = NULL;
    fref->next = NULL;

    if (prev)
        prev->next = next;
    else
        gli_filereflist = next;
    if (next)
        next->prev = prev;
    
    free(fref);
}

void glk_fileref_destroy(fileref_t *fref)
{
    if (!fref) {
        gli_strict_warning("fileref_destroy: invalid ref");
        return;
    }
    gli_delete_fileref(fref);
}

frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock)
{
    char *filename;
    fileref_t *fref;
    
    /* This is a pretty good way to do this on Unix systems. On Macs,
        it's pretty bad, but this library won't be used on the Mac 
        -- I hope. I have no idea about the DOS/Windows world. */
        
    filename = tmpnam(NULL);
    
    fref = gli_new_fileref(filename, usage, rock);
    if (!fref) {
        gli_strict_warning("fileref_create_temp: unable to create fileref.");
        return 0;
    }
    
    return fref;
}

frefid_t glk_fileref_create_by_name(glui32 usage, char *name,
    glui32 rock)
{
    fileref_t *fref;
    char buf[256];
    int len;
    char *cx;
    
    len = strlen(name);
    if (len > 255)
        len = 255;
    
    /* Take out all '/' characters, and make sure the length is greater 
        than zero. Again, this is the right behavior in Unix. 
        DOS/Windows might want to take out '\' instead, unless the
        stdio library converts slashes for you. They'd also want to trim 
        to 8 characters. Remember, the overall goal is to make a legal 
        platform-native filename, without any extra directory 
        components.
       Suffixes are another sore point. Really, the game program 
        shouldn't have a suffix on the name passed to this function. So
        in DOS/Windows, this function should chop off dot-and-suffix,
        if there is one, and then add a dot and a three-letter suffix
        appropriate to the file type (as gleaned from the usage 
        argument.)
    */
    
    memcpy(buf, name, len);
    if (len == 0) {
        buf[0] = 'X';
        len++;
    }
    buf[len] = '\0';
    
    for (cx=buf; *cx; cx++) {
        if (*cx == '/')
            *cx = '-';
    }
    
    fref = gli_new_fileref(buf, usage, rock);
    if (!fref) {
        gli_strict_warning("fileref_create_by_name: unable to create fileref.");
        return 0;
    }
    
    return fref;
}

frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode,
    glui32 rock)
{
    fileref_t *fref;
    struct stat sbuf;
    char buf[256], prbuf[256];
    char *cx;
    int ix, val;
    char *prompt, *prompt2, *lastbuf;
    
    static char lastsavename[256] = "save.gam";
    static char lastscriptname[256] = "script.txt";
    static char lastcmdname[256] = "commands.txt";
    static char lastdataname[256] = "file.dat";
    
    switch (usage & fileusage_TypeMask) {
        case fileusage_SavedGame:
            prompt = "Enter saved game";
            lastbuf = lastsavename;
            break;
        case fileusage_Transcript:
            prompt = "Enter transcript file";
            lastbuf = lastscriptname;
            break;
        case fileusage_InputRecord:
            prompt = "Enter command record file";
            lastbuf = lastcmdname;
            break;
        case fileusage_Data:
        default:
            prompt = "Enter data file";
            lastbuf = lastdataname;
            break;
    }
    
    if (fmode == filemode_Read)
        prompt2 = "to load";
    else
        prompt2 = "to store";
    
    sprintf(prbuf, "%s %s: ", prompt, prompt2);
    
    strcpy(buf, lastbuf);
    val = strlen(buf);
    
    ix = gli_msgin_getline(prbuf, buf, 255, &val);
    if (!ix) {
        /* The player cancelled input. */
        return 0;
    }
    
    buf[val] = '\0';
    
    while (val 
        && (buf[val-1] == '\n' 
            || buf[val-1] == '\r' 
            || buf[val-1] == ' '))
        val--;
    buf[val] = '\0';
    
    for (cx = buf; *cx == ' '; cx++) { }
    
    val = strlen(cx);
    if (!val) {
        /* The player just hit return. */
        return 0;
    }
    
    if (fmode != filemode_Read) {
        if (!stat(cx, &sbuf) && S_ISREG(sbuf.st_mode)) {
            sprintf(prbuf, "Overwrite \"%s\"? [y/n] ", cx);
            while (1) {
                ix = gli_msgin_getchar(prbuf, FALSE);
                if (ix == 'n' || ix == 'N' || ix == '\033' || ix == '\007') {
                    return 0;
                }
                if (ix == 'y' || ix == 'Y') {
                    break;
                }
            }
        }
    }

    strcpy(lastbuf, cx);

    fref = gli_new_fileref(cx, usage, rock);
    if (!fref) {
        gli_strict_warning("fileref_create_by_prompt: unable to create fileref.");
        return 0;
    }
    
    return fref;
}

frefid_t glk_fileref_iterate(fileref_t *fref, glui32 *rock)
{
    if (!fref) {
        fref = gli_filereflist;
    }
    else {
        fref = fref->next;
    }
    
    if (fref) {
        if (rock)
            *rock = fref->rock;
        return fref;
    }
    
    if (rock)
        *rock = 0;
    return NULL;
}

glui32 glk_fileref_get_rock(fileref_t *fref)
{
    if (!fref) {
        gli_strict_warning("fileref_get_rock: invalid ref.");
        return 0;
    }
    
    return fref->rock;
}

glui32 glk_fileref_does_file_exist(fileref_t *fref)
{
    struct stat buf;
    
    if (!fref) {
        gli_strict_warning("fileref_does_file_exist: invalid ref");
        return FALSE;
    }
    
    /* This is sort of Unix-specific, but probably any stdio library
        will implement at least this much of stat(). */
    
    if (stat(fref->filename, &buf))
        return 0;
    
    if (S_ISREG(buf.st_mode))
        return 1;
    else
        return 0;
}

void glk_fileref_delete_file(fileref_t *fref)
{
    if (!fref) {
        gli_strict_warning("fileref_delete_file: invalid ref");
        return;
    }
    
    /* If you don't have the unlink() function, obviously, change it
        to whatever file-deletion function you do have. */
        
    unlink(fref->filename);
}
