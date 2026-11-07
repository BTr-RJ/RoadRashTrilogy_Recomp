#include "sprite_geometry.h"
extern "C" {
#include "genesis_vdp.h"
}
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static GVDPVisualSprite sprites;
static GVDPVisualScroll scroll;
extern "C" void gvdp_set_visual_sprites(GVDPVisualSprite fn,void*){sprites=fn;}
extern "C" void gvdp_set_visual_scroll(GVDPVisualScroll fn,void*){scroll=fn;}
extern "C" void gvdp_set_visual_objects(GVDPHostSprites,void*){}
extern "C" void gvdp_set_visual_surface(GVDPVisualSurface,void*){}
extern "C" void gvdp_set_visual_audit(GVDPVisualAudit,void*){}
extern "C" const uint8_t *gvdp_sprite_patterns(unsigned,unsigned *bytes){*bytes=0;return nullptr;}
extern "C" int gvdp_sprite_pattern_line(unsigned,unsigned){return 0;}
static GVDP v;
static std::array<uint8_t,65536> ram;
static void word(unsigned at,unsigned value){ram[at]=(uint8_t)(value>>8);ram[at+1]=(uint8_t)value;}
static void longword(unsigned at,unsigned value){word(at,value>>16);word(at+2,value);}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"geometry line %d: %s\n",__LINE__,#x);return 1;}}while(0)

// A scaler result in guest staging RAM, subsequently committed to VRAM.
static void object(unsigned frame,unsigned scale,unsigned image=3){
    rr_geometry_frame(frame,1);
    word(0xde64,0);longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x7000,160);word(0x7002,100);word(0x7004,scale);word(0x7008,image);word(0x700a,0x8000);
    rr_geometry_job_begin(0,0xff7000,ram.data());
    word(0x8000,228);word(0x8002,0);word(0x8004,0x8001);word(0x8006,288);
    std::memset(ram.data()+0x9000,0x11,32);word(0xde64,1);
    rr_geometry_job_end(ram.data());
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
    std::memcpy(v.vram+32,ram.data()+0x9000,32);
}
int main(){
    _putenv_s("RR_GEOMETRY","1");
    v.reg[5]=0x60;v.reg[12]=1;v.scanline=100;
    double rect[4];object(0,16384);
    CHECK(sprites&&sprites(nullptr,&v,0,rect));CHECK(rect[0]==160&&rect[2]==168);
    object(4,24576);
    CHECK(sprites(nullptr,&v,0,rect));
    double first=rect[2]-rect[0];CHECK(std::abs(first-64./11.)<1e-9);
    rr_geometry_frame(5,1);CHECK(sprites(nullptr,&v,0,rect));
    CHECK(rect[2]-rect[0]>first&&rect[2]-rect[0]<8);
    // Never attach an old object to a newly uploaded texture in the same SAT.
    v.vram[32]^=1;rr_geometry_frame(6,1);CHECK(!sprites(nullptr,&v,0,rect));
    object(8,24576,4);CHECK(sprites(nullptr,&v,0,rect));CHECK(rect[2]-rect[0]==8);
    v.scanline=152;CHECK(!sprites(nullptr,&v,0,rect));v.scanline=100;
    rr_geometry_frame(9,0);CHECK(!sprites&&!scroll);
    rr_geometry_frame(10,1);CHECK(sprites&&!sprites(nullptr,&v,0,rect));

    // Scroll-only scene updates share one interval across every road row,
    // including frames in which no SAT coordinate or texture changes.
    rr_geometry_reset();v.reg[11]=3;v.reg[13]=0x2c;
    rr_geometry_frame(20,1);int a[4]={0,0,0,0},b[4]={20,20,0,0};
    CHECK(scroll(nullptr,&v,50,a));CHECK(scroll(nullptr,&v,60,b));
    v.vram[0xb001]=8;rr_geometry_frame(24,1);
    int c[4]={8,8,0,0},d[4]={28,28,0,0};
    CHECK(scroll(nullptr,&v,50,c));CHECK(scroll(nullptr,&v,60,d));
    CHECK(c[0]==2&&d[0]==22);
    rr_geometry_frame(25,1);int e[4]={8,8,0,0};CHECK(scroll(nullptr,&v,50,e));CHECK(e[0]==4);
    std::puts("Geometry: committed texture ownership, continuous zoom, pose/pause/HUD resets and synchronized road passed");
}
