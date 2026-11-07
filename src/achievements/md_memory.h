#pragma once
#include <stdint.h>
/* Official Mega Drive sets consume a word-swizzled 64 KiB system RAM view.
   Pure read: never call the bus or change CPU/scheduler state. */
static inline uint32_t rr_md_ra_read(const uint8_t *ram, uint32_t address,
                                    uint8_t *out, uint32_t count) {
    if (!ram || !out || address >= 0x10000u) return 0;
    if (count > 0x10000u - address) count = 0x10000u - address;
    for (uint32_t i = 0; i < count; ++i) out[i] = ram[(address + i) ^ 1u];
    return count;
}
