#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr {
// Immutable source art, before the game's software reduction and SAT clipping.
struct SpriteAsset {
    int width=0,height=0,anchor_x=0,anchor_y=0,palette=0;
    std::vector<uint8_t> pixels;
    explicit operator bool() const {return !pixels.empty();}
};
SpriteAsset decode_sprite(const uint8_t *rom,size_t bytes,unsigned table,
                          unsigned image,bool anchored);
}
