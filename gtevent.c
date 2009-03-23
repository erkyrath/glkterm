/* gtevent.c: Event handling, including glk_select() and timed input code
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.edoc.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef OPT_TIMED_INPUT
#include <sys/time.h>
#endif /* OPT_TIMED_INPUT */

#include <curses.h>
#include "glk.h"
#include "glkterm.h"

/* A pointer to the place where the pending glk_select() will store its
    event. When not inside a glk_select() call, this will be NULL. */
static event_t *curevent = NULL; 

static int halfdelay_running; /* TRUE if halfdelay() has been called. */
static uint32 timing_msec; /* The current timed-event request, exactly as
    passed to glk_request_timer_events(). */

#ifdef OPT_TIMED_INPUT

    /* The time at which the next timed event will occur. This is only valid 
        if timing_msec is nonzero. */
    static struct timeval next_time; 

    static void add_millisec_to_time(struct timeval *tv, uint32 msec);

#endif /* OPT_TIMED_INPUT */

/* Set up the input system. This is called from main(). */
void gli_initialize_events()
{
    halfdelay_running = FALSE;
    
    glk_request_timer_events(0);
}

void glk_select(event_t *event)
{
    int needrefresh = TRUE;
    
    curevent = event;
    gli_event_clearevent(curevent);
    
    gli_windows_update();
    
    while (curevent->type == evtype_None) {
        int key;
    
        if (needrefresh) {
            gli_windows_place_cursor();
            refresh();
            needrefresh = FALSE;
        }
        key = getch();
        
        if (key != ERR) {
            /* An actual key has been hit */
            input_handle_key(key);
            needrefresh = TRUE;
            continue;
        }

        /* key == ERR; it's an idle event */
        
#ifdef OPT_WINCHANGED_SIGNAL
        /* Check to see if the screen-size has changed. The 
            screen_size_changed flag is set by the SIGWINCH signal
            handler. */
        if (screen_size_changed) {
            screen_size_changed = FALSE;
            gli_windows_size_change();
            needrefresh = TRUE;
            continue;
        }
#endif /* OPT_WINCHANGED_SIGNAL */

#ifdef OPT_TIMED_INPUT
        /* Check to see if we've passed next_time. */
        if (timing_msec) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            if (tv.tv_sec > next_time.tv_sec
                || (tv.tv_sec == next_time.tv_sec &&
                    tv.tv_usec > next_time.tv_usec)) {
                next_time = tv;
                add_millisec_to_time(&next_time, timing_msec);
                gli_event_store(evtype_Timer, NULL, 0, 0);
                continue;
            }
        }
#endif /* OPT_TIMED_INPUT */

    }
    
    /* An event has occurred; glk_select() is over. */
    curevent = NULL;
}

/* Various modules can call this to indicate that an event has occurred.
    This doesn't try to queue events, but since a single keystroke or
    idle event can only cause one event at most, this is fine. */
void gli_event_store(uint32 type, window_t *win, uint32 val1, uint32 val2)
{
    if (curevent) {
        curevent->type = type;
        if (win)
            curevent->win = WindowToID(win);
        else
            curevent->win = 0;
        curevent->val1 = val1;
        curevent->val2 = val2;
    }
}

/* The timed-input handling is a little obscure. This is because curses.h
    timed input is a little obscure. As far as I can tell, once you turn
    on timeouts by calling halfdelay(), you can't turn them off again. At
    least not on the OS I tested this on. So we turn on halfdelay() only
    when the program actually requests it by glk_request_timer_events().
    If the program requests that timed events be turned off, we turn the
    halfdelay() timeouts down to once every ten seconds and just ignore
    them.
   To make things worse, timeouts can only be requested with a granularity
    of a tenth of a second. So the best we can normally do is get a
    timeout ten times a second. If the player uses the -precise option,
    we try requesting a timeout delay of *zero* -- this may not be legal,
    and it will certainly waste tremendous amount of CPU time, but it may
    be worth trying.
   To make things even worse than that, if the program is to watch for
    SIGWINCH signals (which indicate that the window has been resized), it
    has to check for these periodically too. So if OPT_WINCHANGED_SIGNAL
    is defined, we turn on halfdelay() even if the program doesn't want
    timer events. We use a timeout of half a second in this case. 
*/
    
void glk_request_timer_events(uint32 millisecs)
{
    int delay;
    
    timing_msec = millisecs;
    
    if (timing_msec == 0) {
        /* turn off */
        if (halfdelay_running)
            delay = 100; /* ten seconds */
    }
    else {
        /* turn on */
    
#ifdef OPT_TIMED_INPUT
        halfdelay_running = TRUE;
        
        gettimeofday(&next_time, NULL);
        add_millisec_to_time(&next_time, timing_msec);
        
        if (pref_precise_timing)
            delay = 0;
        else
            delay = 1;
            
#else /* !OPT_TIMED_INPUT */

        /* Can't really turn on timing, so pretend it was turned off. */
        if (halfdelay_running)
            delay = 100; /* ten seconds */

#endif /* OPT_TIMED_INPUT */
    }

#ifdef OPT_WINCHANGED_SIGNAL
    if (!halfdelay_running || delay > 5) {
        halfdelay_running = TRUE;
        delay = 5; /* half a second */
    }
#endif /* OPT_WINCHANGED_SIGNAL */

    if (halfdelay_running)
        halfdelay(delay);
}

#ifdef OPT_TIMED_INPUT

/* Given a time value, add a fixed delay to it. */
static void add_millisec_to_time(struct timeval *tv, uint32 msec)
{
    int sec;
    
    sec = msec / 1000;
    msec -= sec*1000;
    
    tv->tv_sec += sec;
    tv->tv_usec += (msec * 1000);
    
    if (tv->tv_usec >= 1000000) {
        tv->tv_usec -= 1000000;
        tv->tv_sec++;
    }
}

#endif /* OPT_TIMED_INPUT */
