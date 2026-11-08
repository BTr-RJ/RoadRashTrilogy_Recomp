#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void rr_local_init(const char *game,int persist);
void rr_local_frame(int racing,unsigned speed,unsigned track,int paused);
void rr_local_toggle(void);
void rr_local_state_loaded(void);
int rr_local_panel(void);
const char *rr_local_title(unsigned index);
int rr_local_unlocked(unsigned index);
const char *rr_local_toast(void);
unsigned rr_local_count(void);
#ifdef __cplusplus
}
#endif
