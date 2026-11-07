#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Decode locally downloaded AAC/MP3/WAV using Windows' installed codecs.
   All entry points run on the main simulation thread. */
void rr_music_init(const char *game);
int rr_music_start(unsigned cue);
void rr_music_stop(void);
void rr_music_pause(int paused);
void rr_music_levels(int music_percent,int effects_percent);
double rr_music_position(void);
int rr_music_replacing(void);
void rr_music_mix(int16_t *stereo,size_t frames,double rate);
const char *rr_music_status(void);
void rr_music_shutdown(void);
#ifdef __cplusplus
}
#endif
