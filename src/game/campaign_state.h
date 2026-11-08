#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void rr_campaign_init(const char *game);
extern char rr_campaign_build_id[65];
const char *rr_campaign_unavailable(void);
void rr_campaign_observe(int racing,int campaign,int player_input);
void rr_campaign_input(void);
void rr_campaign_tick(void);
int rr_campaign_load(void);
void rr_campaign_notice(const char *message);
const char *rr_campaign_message(void);
size_t rr_campaign_state_size(void);
int rr_campaign_state_save(void *data,size_t size);
int rr_campaign_state_load(const void *data,size_t size,int apply);
void rr_adapter_state(uint64_t *irq_epoch,unsigned *music_cue,int apply);
#ifdef __cplusplus
}
#endif
