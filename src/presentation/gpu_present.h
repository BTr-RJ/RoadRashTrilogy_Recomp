#pragma once
#include <stdint.h>
struct SDL_Renderer;
#ifdef __cplusplus
extern "C" {
#endif
int rr_gpu_present(struct SDL_Renderer *renderer,const uint32_t *pixels,int stride,int width,int height,int native_width,uint64_t frame);
void rr_gpu_shutdown(void);
void rr_present_capture(struct SDL_Renderer *renderer,uint64_t frame);
#ifdef __cplusplus
}
#endif
