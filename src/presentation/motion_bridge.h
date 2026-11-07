#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void rr_motion_scene(int active,uint64_t picture);
const uint32_t *rr_motion_present(const uint32_t *input,int stride,int width,int height,uint64_t frame);
#ifdef __cplusplus
}
#endif
