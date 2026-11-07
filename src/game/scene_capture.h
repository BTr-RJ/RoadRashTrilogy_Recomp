#pragma once
#include <stdint.h>
/* Engineering trace of the game's actual projection stage. Read-only: this
 * is the source adapter for continuous object rendering, not image flow. */
void rr_scene_capture(const char *game,uint64_t tick,uint32_t actor,uint32_t job,
                      uint32_t camera,const uint8_t *ram,unsigned stride);
void rr_sprite_job_begin(const char *game,uint64_t tick,uint32_t job,
                         const uint8_t *ram,unsigned sat_count,unsigned sat_base);
void rr_sprite_job_end(const uint8_t *ram,unsigned sat_count);
