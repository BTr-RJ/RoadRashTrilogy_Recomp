#include "sprite_assets.h"

namespace rr {
SpriteAsset decode_sprite(const uint8_t *rom,size_t bytes,unsigned table,
                          unsigned image,bool anchored){
    SpriteAsset out;
    size_t entry=(size_t)table+(size_t)image*4;
    if(!rom||entry+4>bytes)return out;
    size_t at=((uint32_t)rom[entry]<<24)|((uint32_t)rom[entry+1]<<16)|
              ((uint32_t)rom[entry+2]<<8)|rom[entry+3];
    if(at+6>bytes)return out;
    int ax=0,ay=0;
    if(anchored){ax=(int8_t)rom[at];ay=(int8_t)rom[at+1];at+=2;}
    unsigned width=rom[at],height=rom[at+1];
    // Without explicit anchor bytes the native scaler centers the image at
    // descriptor X and places its bottom at descriptor Y ($9836 in RR1).
    // A top-left default put people/cars entirely below their hill clip.
    if(!anchored){ax=(int)width/2;ay=(int)height;}
    // 68000 scaler $9168/$9172: ceil(width/2) bytes plus one guard byte.
    // The guard must not become a texel or a one-byte drift on every row.
    size_t stride=(width+1)/2+1;
    if(!width||!height||at+4+stride*height>bytes)return out;
    out.width=width;out.height=height;out.anchor_x=ax;out.anchor_y=ay;
    out.palette=(rom[at+2]>>5)&3;out.pixels.resize(width*height);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        unsigned byte=rom[at+4+y*stride+x/2];
        out.pixels[y*width+x]=(uint8_t)((byte>>((x&1)?0:4))&15);
    }
    out.coverage.build(out.width,out.height,out.pixels.data());
    return out;
}
}
