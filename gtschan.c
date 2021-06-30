/* gtschan.c: Sound channel objects
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#include "gtoption.h"
#include <stdio.h>
#include "glk.h"
#include "glkterm.h"

#ifdef GLK_MODULE_SOUND

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include "gi_blorb.h"
#include "queue.h"

#define GLK_MAX_VOLUME 0x10000
#define GLK_SOUND_FREQUENCY 44100
#define GLK_SOUND_CHANNELS 2
#define GLK_SOUND_CHUNK_SIZE 4096
#define GLK_SOUND_MIX_CHANNELS 64

typedef struct otherres_struct {
    glui32 snd;
    Mix_Chunk *mix_chunk;
    TAILQ_ENTRY(otherres_struct) entries;
} otherres_t;
static TAILQ_HEAD(unused1, otherres_struct) otherres_list =
    TAILQ_HEAD_INITIALIZER(otherres_list);

static SDL_mutex *mutex = NULL;
static TAILQ_HEAD(unused2, glk_schannel_event_struct) schannel_events =
    TAILQ_HEAD_INITIALIZER(schannel_events);
static TAILQ_HEAD(unused3, glk_schannel_struct) schannels =
    TAILQ_HEAD_INITIALIZER(schannels);

/* static functions */
#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
static void warningf(const char *format, ...);

/* callbacks */
void gli_finished_callback(int mix_channel);
Uint32 gli_volume_callback(Uint32 interval, void *param);

static int glkvolume_to_mixvolume(glui32 glkvolume)
{
    const double ratio = (double)glkvolume / GLK_MAX_VOLUME;
    return (glkvolume >= GLK_MAX_VOLUME) ? MIX_MAX_VOLUME :
        (int)(ratio * ratio * MIX_MAX_VOLUME);
}

static int load_resource_from_dir(glui32 snd, const char *dir,
                                  Mix_Chunk **pmix_chunk)
{
    size_t len;
    char *fullpath;
    Mix_Chunk *mix_chunk;
    otherres_t *otherres;

    len = strlen(dir) + 32;
    fullpath = malloc(len);
    if (!fullpath) {
        return FALSE;
    }
    snprintf(fullpath, len, "%s/SND%d", (dir && *dir) ? dir : ".", snd);
    mix_chunk = Mix_LoadWAV(fullpath);
    if (!mix_chunk) {
        free(fullpath);
        return FALSE;
    }
    otherres = malloc(sizeof(otherres_t));
    if (!otherres) {
        free(fullpath);
        Mix_FreeChunk(mix_chunk);
        return FALSE;
    }
    *pmix_chunk = mix_chunk;

    /* Remember this resource. */
    otherres->snd = snd;
    otherres->mix_chunk = mix_chunk;
    TAILQ_INSERT_TAIL(&otherres_list, otherres, entries);

    free(fullpath);
    return TRUE;
}

static int load_resource(glui32 snd, Mix_Chunk **pmix_chunk)
{
    giblorb_map_t *map = giblorb_get_resource_map();
    otherres_t *otherres = NULL;
    strid_t stream = NULL;

    /* If we have a Blorb, we use that. */
    if (map) {
        giblorb_result_t res = {0};
        if (giblorb_load_resource(map, giblorb_method_Memory, &res,
                                  giblorb_ID_Snd, snd) != giblorb_err_None) {
            warningf("sound: unable to load resource %d", snd);
            return FALSE;
        }
        if (res.length > INT_MAX) {
            return FALSE;
        }
        *pmix_chunk = Mix_LoadWAV_RW(
            SDL_RWFromMem(res.data.ptr, (int)res.length), TRUE);
        return TRUE;
    }

    /* If the resource map hasn't been set, we might still have sound files
        alongside the game file.
        https://www.eblong.com/zarf/glk/Glk-Spec-075.html#blorb_user
        https://www.eblong.com/zarf/blorb/blorb.html#s16 */

    /* Have we already loaded this one? */
    for (otherres = TAILQ_FIRST(&otherres_list); otherres;
         otherres = TAILQ_NEXT(otherres, entries)) {
        if (otherres->snd == snd) {
            *pmix_chunk = otherres->mix_chunk;
            return TRUE;
        }
    }

    /* Try the working directory first. */
    if (load_resource_from_dir(snd, gli_workingdir, pmix_chunk)) {
        return TRUE;
    }

    /* Look at the file names for open streams to find possible
        locations of additional resource files. */
    for (stream = glk_stream_iterate(NULL, NULL); stream;
         stream = glk_stream_iterate(stream, NULL)) {
        char *dir = NULL;
        char *slash = NULL;
        int ret;

        if (!stream->filename) {
            continue;
        }

        /* Create a full path to what might be a sound file. */
        /* Include space for the number. */
        dir = strdup(stream->filename);
        slash = strrchr(dir, '/');
        if (slash) {
            *slash = 0;
        } else {
            *dir = 0;
        }

        ret = load_resource_from_dir(snd, dir, pmix_chunk);
        free(dir);
        return ret;
    }
    warningf("sound: unable to load resource %d", snd);
    return FALSE;
}

static schannel_t *new_schannel(glui32 volume, glui32 rock)
{
    schannel_t *chan = malloc(sizeof(schannel_t));
    if (!chan)
        return NULL;

    chan->magicnum = MAGIC_SCHANNEL_NUM;
    chan->rock = rock;

    chan->finished_event_data = NULL;
    chan->mix_channel = -1;
    chan->mix_chunk = NULL;
    chan->paused = FALSE;
    chan->volume_sdl_timerid = 0;
    chan->volume_begin = 0;
    chan->volume_current = volume;
    chan->volume_end = 0;
    chan->volume_event_data = NULL;
    chan->volume_ticks_begin = 0;
    chan->volume_ticks_duration = 0;
    chan->volume_sdl_timerid = 0;

    SDL_LockMutex(mutex);
    TAILQ_INSERT_TAIL(&schannels, chan, entries);

    if (gli_register_obj)
        chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
    SDL_UnlockMutex(mutex);
    return chan;
}

static int play_resource(schannel_t *chan, Mix_Chunk *mix_chunk,
                         glui32 snd, glui32 repeats, glui32 notify)
{
    int success = FALSE;

    SDL_LockMutex(mutex);
    if (notify) {
        chan->finished_event_data = malloc(sizeof(schannel_event_t));
        if (!chan->finished_event_data) {
            warningf("sound: unable to allocate memory");
            return FALSE;
        }
        chan->finished_event_data->event.type = evtype_SoundNotify;
        chan->finished_event_data->event.win = NULL;
        chan->finished_event_data->event.val1 = snd;
        chan->finished_event_data->event.val2 = notify;
        chan->finished_event_data->chan = chan;
    }

    do {
        chan->mix_chunk = mix_chunk;
        chan->mix_channel = Mix_PlayChannel(-1, chan->mix_chunk, repeats - 1);
        if (chan->mix_channel < 0) {
            warningf("sound: error playing resource: %s", Mix_GetError());
            chan->mix_chunk = NULL;
            break;
        }
        Mix_Volume(chan->mix_channel,
                   glkvolume_to_mixvolume(chan->volume_current));
        if (chan->paused) {
            Mix_Pause(chan->mix_channel);
        }
        success = TRUE;
        break;
    } while (0);
    SDL_UnlockMutex(mutex);
    return success;
}

static void set_volume(schanid_t chan, glui32 vol, glui32 duration,
                       glui32 notify)
{
    if (!chan) {
        warningf("sound: invalid channel");
        return;
    }

    SDL_LockMutex(mutex);
    if (duration == 0) {
        chan->volume_current = vol;
        if (chan->mix_chunk) {
            Mix_Volume(chan->mix_channel,
                       glkvolume_to_mixvolume(chan->volume_current));
        }
    } else {
        if (notify) {
            chan->volume_event_data = malloc(sizeof(schannel_event_t));
            if (!chan->volume_event_data) {
                return;
            }
            chan->volume_event_data->event.type = evtype_VolumeNotify;
            chan->volume_event_data->event.win = NULL;
            chan->volume_event_data->event.val1 = 0;
            chan->volume_event_data->event.val2 = notify;
            chan->volume_event_data->chan = chan;
        }
        chan->volume_begin = chan->volume_current;
        chan->volume_end = vol;
        chan->volume_ticks_begin = SDL_GetTicks();
        chan->volume_ticks_duration = duration;
        if (chan->volume_sdl_timerid) {
            SDL_RemoveTimer(chan->volume_sdl_timerid);
        }
        chan->volume_sdl_timerid = SDL_AddTimer(100, &gli_volume_callback,
                                                chan);
    }
    SDL_UnlockMutex(mutex);
}

static void warningf(const char *format, ...)
{
    va_list args;
    int n = 0;
    size_t siz = 256;
    char *buf = NULL;
    for (;;) {
        buf = malloc(siz);
        if (!buf) {
            gli_strict_warning((char *)format);
            return;
        }
        va_start(args, format);
        n = vsnprintf(buf, siz, format, args);
        va_end(args);
        if (n < 0 || (size_t)n < siz) {
            gli_strict_warning(buf);
            free(buf);
            return;
        }
        siz = (size_t)n + 1;
        free(buf);
    }
}

void gli_initialize_sound(void)
{
    if (mutex) {
        warningf("sound: already initialized");
        return;
    }
    mutex = SDL_CreateMutex();
    if (!mutex) {
        warningf("sound: error in SDL_CreateMutex: %s", SDL_GetError());
        return;
    }
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        return;
    }
    /* Ignore the return because we may still have enough sound support. */
    Mix_Init(MIX_INIT_MOD | MIX_INIT_OGG);
    if (Mix_OpenAudio(GLK_SOUND_FREQUENCY, MIX_DEFAULT_FORMAT,
                      GLK_SOUND_CHANNELS, GLK_SOUND_CHUNK_SIZE) < 0) {
        return;
    }
    Mix_ChannelFinished(gli_finished_callback);
    Mix_AllocateChannels(GLK_SOUND_MIX_CHANNELS);
}

void gli_shutdown_sound(void)
{
    schannel_t *chan = NULL;
    schannel_event_t *event = NULL;
    otherres_t *otherres = NULL;

    if (!mutex) {
        warningf("sound: not initialized");
        return;
    }

    SDL_LockMutex(mutex);
    chan = TAILQ_FIRST(&schannels);
    while (chan) {
        schannel_t *tmp = TAILQ_NEXT(chan, entries);
        glk_schannel_destroy(chan);
        chan = tmp;
    }
    event = TAILQ_FIRST(&schannel_events);
    while (event) {
        schannel_event_t *tmp = TAILQ_NEXT(event, entries);
        TAILQ_REMOVE(&schannel_events, event, entries);
        free(event);
        event = tmp;
    }
    otherres = TAILQ_FIRST(&otherres_list);
    while (otherres) {
        otherres_t *tmp = TAILQ_NEXT(otherres, entries);
        Mix_FreeChunk(otherres->mix_chunk);
        free(otherres);
        otherres = tmp;
    }
    SDL_UnlockMutex(mutex);

    SDL_DestroyMutex(mutex);
    mutex = NULL;
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
}

glui32 gli_sound_gestalt(glui32 id)
{
    switch (id) {
    case gestalt_Sound:
    case gestalt_Sound2:
    case gestalt_SoundMusic:
    case gestalt_SoundNotify:
    case gestalt_SoundVolume:
        return pref_sound ? 1 : 0;
    default:
        return FALSE;
    }
}

void gli_store_sound_events(void)
{
    schannel_event_t *event_data = NULL, *tmp = NULL;

    SDL_LockMutex(mutex);
    TAILQ_FOREACH_SAFE(event_data, &schannel_events, entries, tmp) {
        gli_event_store(event_data->event.type, event_data->event.win,
                        event_data->event.val1, event_data->event.val2);
        TAILQ_REMOVE(&schannel_events, event_data, entries);
        free(event_data);
    }
    SDL_UnlockMutex(mutex);
}

void gli_finished_callback(int mix_channel)
{
    schannel_t *chan = NULL;

    SDL_LockMutex(mutex);
    TAILQ_FOREACH(chan, &schannels, entries) {
        if (chan->mix_channel != mix_channel) {
            continue;
        }
        if (chan->finished_event_data) {
            TAILQ_INSERT_TAIL(&schannel_events, chan->finished_event_data,
                              entries);
            chan->finished_event_data = NULL;
            break;
        }
    }
    SDL_UnlockMutex(mutex);
}

Uint32 gli_volume_callback(Uint32 interval, void *param)
{
    schannel_t *chan = param;
    Uint32 ticks_passed = 0;
    double progress = 0.;

    SDL_LockMutex(mutex);
    ticks_passed = SDL_GetTicks() - chan->volume_ticks_begin;
    progress = (double)(ticks_passed) / chan->volume_ticks_duration;
    if (progress >= 0 && progress < 1) {
        glsi32 total_increase = chan->volume_end - chan->volume_begin;
        chan->volume_current =
            (glui32)(progress * total_increase + chan->volume_begin);
    } else {
        SDL_RemoveTimer(chan->volume_sdl_timerid);
        chan->volume_sdl_timerid = 0;
        chan->volume_current = chan->volume_end;
        if (chan->volume_event_data) {
            TAILQ_INSERT_TAIL(&schannel_events, chan->volume_event_data,
                              entries);
            chan->volume_event_data = NULL;
        }
    }
    if (chan->mix_chunk) {
        Mix_Volume(chan->mix_channel,
                   glkvolume_to_mixvolume(chan->volume_current));
    }
    SDL_UnlockMutex(mutex);
    return interval;
}

schanid_t glk_schannel_create(glui32 rock)
{
    return new_schannel(GLK_MAX_VOLUME, rock);
}

void glk_schannel_destroy(schanid_t chan)
{
    if (!chan) {
        warningf("sound: invalid channel");
        return;
    }

    if (gli_unregister_obj)
        (*gli_unregister_obj)(chan, gidisp_Class_Schannel, chan->disprock);

    chan->magicnum = 0;

    glk_schannel_stop(chan);

    SDL_LockMutex(mutex);
    TAILQ_REMOVE(&schannels, chan, entries);
    free(chan);
    SDL_UnlockMutex(mutex);
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rockptr)
{
    SDL_LockMutex(mutex);
    if (!chan) {
        chan = TAILQ_FIRST(&schannels);
    }
    else {
        chan = TAILQ_NEXT(chan, entries);
    }

    if (chan) {
        if (rockptr)
            *rockptr = chan->rock;
        SDL_UnlockMutex(mutex);
        return chan;
    }

    if (rockptr)
        *rockptr = 0;
    SDL_UnlockMutex(mutex);
    return NULL;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    if (!chan) {
        warningf("sound: invalid channel");
        return 0;
    }
    return chan->rock;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    return glk_schannel_play_ext(chan, snd, 1, 0);
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats,
    glui32 notify)
{
    Mix_Chunk *mix_chunk = NULL;

    if (!chan) {
        warningf("sound: invalid channel");
        return 0;
    }

    glk_schannel_stop(chan);
    if (repeats == 0) {
        return 1;
    }
    if (!load_resource(snd, &mix_chunk)) {
        return 0;
    }
    if (!play_resource(chan, mix_chunk, snd, repeats, notify)) {
        return 0;
    }
    return 1;
}

void glk_schannel_stop(schanid_t chan)
{
    schannel_event_t *event_data = NULL, *tmp = NULL;

    if (!chan) {
        warningf("sound: invalid channel");
        return;
    }

    /* Remove pending events for the given channel. */
    TAILQ_FOREACH_SAFE(event_data, &schannel_events, entries, tmp) {
        if (event_data->chan == chan) {
            TAILQ_REMOVE(&schannel_events, event_data, entries);
            free(event_data);
        }
    }

    SDL_LockMutex(mutex);
    if (chan->finished_event_data) {
        free(chan->finished_event_data);
        chan->finished_event_data = NULL;
    }
    if (chan->volume_sdl_timerid) {
        SDL_RemoveTimer(chan->volume_sdl_timerid);
        chan->volume_sdl_timerid = 0;
    }
    if (chan->volume_event_data) {
        free(chan->volume_event_data);
        chan->volume_event_data = NULL;
    }
    if (chan->mix_chunk) {
        int mix_channel = chan->mix_channel;
        chan->mix_channel = -1;
        chan->mix_chunk = NULL;
        Mix_HaltChannel(mix_channel);
    }
    SDL_UnlockMutex(mutex);
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    set_volume(chan, vol, 0, 0);
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    if (flag) {
        Mix_Chunk *mix_chunk = NULL;
        load_resource(snd, &mix_chunk);
    } else {
        /* We don't unload resources from a Blorb. */
        otherres_t *otherres = NULL;
        for (otherres = TAILQ_FIRST(&otherres_list); otherres;
             otherres = TAILQ_NEXT(otherres, entries)) {
            if (otherres->snd == snd) {
                TAILQ_REMOVE(&otherres_list, otherres, entries);
                Mix_FreeChunk(otherres->mix_chunk);
                free(otherres);
                return;
            }
        }
    }
}

#ifdef GLK_MODULE_SOUND2

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    return new_schannel(volume, rock);
}

glui32 glk_schannel_play_multi(schanid_t *chanarray, glui32 chancount,
  glui32 *sndarray, glui32 soundcount, glui32 notify)
{
    glui32 ret = 0;
    glui32 i;

    for (i = 0; i < chancount; ++i) {
        if (i >= soundcount) {
            break;
        }
        ret += glk_schannel_play_ext(chanarray[i], sndarray[i], 1, notify);
    }
    return ret;
}

void glk_schannel_pause(schanid_t chan)
{
    if (!chan) {
        warningf("sound: invalid channel");
        return;
    }

    chan->paused = TRUE;
    if (chan->mix_chunk) {
        Mix_Pause(chan->mix_channel);
    }
}

void glk_schannel_unpause(schanid_t chan)
{
    if (!chan) {
        warningf("sound: invalid channel");
        return;
    }

    chan->paused = FALSE;
    if (chan->mix_chunk) {
        Mix_Resume(chan->mix_channel);
    }
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol,
                                 glui32 duration, glui32 notify)
{
    set_volume(chan, vol, duration, notify);
}

#endif /* GLK_MODULE_SOUND2 */

#endif /* GLK_MODULE_SOUND */
