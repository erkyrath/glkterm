/* gtevent.c: Event handling, including glk_select() and timed input code
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
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
static glui32 timing_msec; /* The current timed-event request, exactly as
    passed to glk_request_timer_events(). */

#ifdef OPT_TIMED_INPUT

    /* The time at which the next timed event will occur. This is only valid 
        if timing_msec is nonzero. */
    static struct timeval next_time; 

    static void add_millisec_to_time(struct timeval *tv, glui32 msec);

#endif /* OPT_TIMED_INPUT */

/* Set up the input system. This is called from main(). */
void gli_initialize_events()
{
    halfdelay_running = FALSE;
    timing_msec = 0;

    gli_set_halfdelay();
}

void glk_select(event_t *event)
{
    int needrefresh = TRUE;
    
    curevent = event;
    gli_event_clearevent(curevent);
    
    gli_windows_update();
    gli_windows_set_paging(FALSE);
    gli_input_guess_focus();
    
    while (curevent->type == evtype_None) {
        int key;
    
        /* It would be nice to display a "hit any key to continue" message in
            all windows which require it. */
        if (needrefresh) {
            gli_windows_place_cursor();
            refresh();
            needrefresh = FALSE;
        }
        key = getch();
        
#ifdef OPT_USE_SIGNALS
        if (just_killed) {
            /* Someone hit ctrl-C. This flag is set by the
                SIGINT / SIGHUP signal handlers.*/
            gli_fast_exit();
        }
#endif /* OPT_USE_SIGNALS */
        
        if (key != ERR) {
            /* An actual key has been hit */
            gli_input_handle_key(key);
            needrefresh = TRUE;
            continue;
        }

        /* key == ERR; it's an idle event */
        
#ifdef OPT_USE_SIGNALS

        /* Check to see if the program has just resumed. This 
            flag is set by the SIGCONT signal handler. */
        if (just_resumed) {
            just_resumed = FALSE;
            gli_set_halfdelay();
            needrefresh = TRUE;
            continue;
        }

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

#endif /* OPT_USE_SIGNALS */

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
    gli_windows_trim_buffers();
    curevent = NULL;
}

void glk_select_poll(event_t *event)
{
    int firsttime = TRUE;
    
    curevent = event;
    gli_event_clearevent(curevent);
    
    gli_windows_update();
    
    /* Now we check, once, all the stuff that glk_select() checks
        periodically. This includes rearrange events and timer events. 
       Yes, this looks like a loop, but that's just so we can use
        continue; it executes exactly once. */
        
    while (firsttime) {
        firsttime = FALSE;

        gli_windows_place_cursor();
        refresh();
        
#ifdef OPT_USE_SIGNALS

        /* We don't need to check to see if the program has just resumed. 
            The only reason glk_select() does that is to refresh the screen,
            and that's just been done anyhow. */

#ifdef OPT_WINCHANGED_SIGNAL
        /* Check to see if the screen-size has changed. The 
            screen_size_changed flag is set by the SIGWINCH signal
            handler. */
        if (screen_size_changed) {
            screen_size_changed = FALSE;
            gli_windows_size_change();
            continue;
        }
#endif /* OPT_WINCHANGED_SIGNAL */

#endif /* OPT_USE_SIGNALS */

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

    curevent = NULL;
}

/* Various modules can call this to indicate that an event has occurred.
    This doesn't try to queue events, but since a single keystroke or
    idle event can only cause one event at most, this is fine. */
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2)
{
    if (curevent) {
        curevent->type = type;
        curevent->win = win;
        curevent->val1 = val1;
        curevent->val2 = val2;
    }
}

void glk_request_timer_events(glui32 millisecs)
{
    timing_msec = millisecs;
    gli_set_halfdelay();
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
    
void gli_set_halfdelay()
{
    /* If there's no timed input, we don't call halfdelay() at all. Not
        a bit. */
    
#ifdef OPT_TIMED_INPUT

    int delay;
    
    if (timing_msec == 0) {
        /* turn off */
        if (halfdelay_running)
            delay = 100; /* ten seconds */
    }
    else {
        /* turn on */
        halfdelay_running = TRUE;
        
        gettimeofday(&next_time, NULL);
        add_millisec_to_time(&next_time, timing_msec);
        
        if (pref_precise_timing)
            delay = 0;
        else
            delay = 1;
            
    }

#ifdef OPT_USE_SIGNALS
    if (!halfdelay_running || delay > 5) {
        halfdelay_running = TRUE;
        delay = 5; /* half a second */
    }
#endif /* OPT_USE_SIGNALS */

    if (halfdelay_running)
        halfdelay(delay);

#endif /* OPT_TIMED_INPUT */
}

#ifdef OPT_TIMED_INPUT

/* Given a time value, add a fixed delay to it. */
static void add_millisec_to_time(struct timeval *tv, glui32 msec)
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

