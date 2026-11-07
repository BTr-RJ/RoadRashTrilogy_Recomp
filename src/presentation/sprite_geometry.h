#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// scene: 0 = native screen, 1 = live single-player race, 2 = paused race.
// Paused wide views retain spatial road continuation, without motion/zoom.
void rr_geometry_frame(uint64_t frame,int scene);
void rr_geometry_job_begin(int game,uint32_t job,const uint8_t *ram);
void rr_geometry_job_end(const uint8_t *ram);
void rr_geometry_reset(void);
void rr_geometry_source(int game,const uint8_t *rom,unsigned bytes,const uint8_t *ram);
void rr_geometry_project(int game,uint32_t job,uint32_t source,uint32_t z,int x,int y);
void rr_geometry_actor(int game,uint32_t job,uint32_t actor,const uint8_t *ram);
void rr_geometry_actor_begin(int game,uint32_t job,uint32_t actor);
void rr_geometry_actor_end(int game,uint32_t next_job,const uint8_t *ram);
void rr_geometry_scenery(int game,uint32_t job,uint32_t source,unsigned placement,const uint8_t *ram);
#ifdef __cplusplus
}
#endif
