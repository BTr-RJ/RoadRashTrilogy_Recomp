#pragma once
/* Both H32 and H40 images target a 4:3 CRT. Expanded columns keep the same
 * pixel size as the native image. Never infer pixel aspect from output width. */
static inline double rr_display_aspect(int source_width,int native_width) {
    return (native_width>0&&source_width>0)?4.0*source_width/(3.0*native_width):4.0/3.0;
}
static inline int rr_wide_margin(int native_width) {
    return (native_width+3)/6;
}
