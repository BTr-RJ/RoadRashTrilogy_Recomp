#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct RRRa RRRa;
typedef uint32_t (*RRRaRead)(uint32_t address, uint8_t *out, uint32_t count, void *user);
RRRa *rr_ra_create(const wchar_t *credential_file, RRRaRead read, void *user);
RRRa *rr_ra_game_create(const uint8_t *rom, size_t size, RRRaRead read, void *user);
void rr_ra_destroy(RRRa *service);
void rr_ra_login(RRRa *service, const char *username, const char *password);
int rr_ra_restore_login(RRRa *service);
void rr_ra_bind_rom(RRRa *service, const uint8_t *rom, size_t size);
void rr_ra_update(RRRa *service, int evaluate_committed_frame);
const char *rr_ra_status(const RRRa *service);
const char *rr_ra_notification(const RRRa *service);
int rr_ra_logged_in(const RRRa *service);
int rr_ra_loaded(const RRRa *service);
void rr_ra_logout(RRRa *service);
#ifdef __cplusplus
}
#endif
