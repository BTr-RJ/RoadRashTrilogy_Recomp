#include "sprite_coverage.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"coverage line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    std::array<uint32_t,256> palette{};palette.fill(0xff000000);palette[1]=0xffffffff;palette[2]=0xffff0000;palette[3]=0xff00ff00;
    uint8_t pixel=1;rr::CoverageImage dot;dot.build(1,1,&pixel);
    uint8_t base[8]{},owner[8]{},plane[8]{},mask[8]{};uint32_t out[8]{};
    std::vector<rr::CoverageSprite> sprites{{&dot,2.25,0,1,1,0,2,false,false,true,false,false}};
    rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);
    CHECK(mask[2]&&mask[3]&&!mask[1]&&!mask[4]);CHECK((out[2]&255)==191&&(out[3]&255)==64);
    // Subpixel movement has continuous area, not a one-pixel jump.
    int last=-1;for(int step=0;step<=32;++step){sprites[0].left=2.+step/32.;
        rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);
        int right=mask[3]?(int)(out[3]&255):0;CHECK(right>=last);if(last>=0)CHECK(right-last<=8);last=right;
    }
    // A very thin feature remains visible for every fractional position.
    sprites[0].scale=.1;sprites[0].top=.4;
    for(int step=0;step<100;++step){sprites[0].left=2.+step/100.;
        rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);
        int energy=0;for(int x=0;x<8;++x)if(mask[x])energy+=out[x]&255;CHECK(energy>=2&&energy<=3);
    }
    // SAT order protects a native player; planes protect low-priority art.
    sprites[0]={&dot,2.25,0,1,1,0,2,false,false,true,false,false};owner[2]=1;
    rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);CHECK(!mask[2]&&mask[3]);
    owner[2]=0;plane[2]=1;sprites[0].priority=false;
    rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);CHECK(!mask[2]);
    plane[2]=0;sprites[0].priority=true;
    // Fractional crest clipping does not move with the sprite anchor.
    sprites[0].clip=.5;
    rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);CHECK((out[2]&255)==96);
    // Area integration of a sparse large texture uses the same exact result.
    std::vector<uint8_t> texture(64*64);texture[0]=1;rr::CoverageImage sparse;sparse.build(64,64,texture.data());
    auto sample=sparse.area(0,0,64,64,64.*64,palette.data(),0,false);CHECK(std::abs(sample.a-1./4096)<1e-12);
    sample=sparse.area(.5,0,64,64,64.*64,palette.data(),0,false);CHECK(std::abs(sample.a-.5/4096)<1e-12);
    // Overlapping body/weapon parts keep first-wins native SAT ordering.
    uint8_t red=2,green=3;rr::CoverageImage a,b;a.build(1,1,&red);b.build(1,1,&green);
    sprites={{&a,2,0,1,1,0,1},{&b,2,0,1,1,0,2}};
    rr::resolve_sprite_row(0,0,8,base,owner,plane,palette.data(),sprites,out,mask);CHECK(out[2]==0xffff0000);
    // No coverage may escape the active view or cross the fixed HUD boundary.
    rr::resolve_sprite_row(0,3,8,base,owner,plane,palette.data(),sprites,out,mask);for(int x=3;x<8;++x)CHECK(!mask[x]);
    rr::resolve_sprite_row(1,0,8,base,owner,plane,palette.data(),sprites,out,mask);for(int x=0;x<8;++x)CHECK(!mask[x]);
    std::puts("Sprite coverage: fractional motion, thin objects, current-color area, SAT/plane order and fixed clipping passed");
}
