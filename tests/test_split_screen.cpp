#include "sprite_geometry.h"
#include "race_view.h"
extern "C" {
#include "genesis_vdp.h"
}
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static GVDPVisualSprite sprites;
static GVDPHostSprites objects;
static GVDPVisualScroll scroll;
static GVDPVisualViewport viewport;
extern "C" void gvdp_set_visual_sprites(GVDPVisualSprite fn,void*){sprites=fn;}
extern "C" void gvdp_set_visual_scroll(GVDPVisualScroll fn,void*){scroll=fn;}
extern "C" void gvdp_set_visual_objects(GVDPHostSprites fn,void*){objects=fn;}
extern "C" void gvdp_set_visual_surface(GVDPVisualSurface,void*){}
extern "C" void gvdp_set_visual_audit(GVDPVisualAudit,void*){}
extern "C" void gvdp_set_visual_viewport(GVDPVisualViewport fn,void*){viewport=fn;}
extern "C" const uint8_t *gvdp_sprite_patterns(unsigned,unsigned *bytes){*bytes=0;return nullptr;}
extern "C" int gvdp_sprite_pattern_line(unsigned,unsigned){return 0;}
extern "C" const uint32_t *gvdp_host_palette(void){return nullptr;}
static std::array<uint8_t,65536> ram;
static GVDP v;
static void word(unsigned at,unsigned value){ram[at&65535]=(uint8_t)(value>>8);ram[(at+1)&65535]=(uint8_t)value;}
static void longword(unsigned at,unsigned value){word(at,value>>16);word(at+2,value);}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"split line %d: %s\n",__LINE__,#x);return 1;}}while(0)

static void road(const rr::RaceView& view,double camera){
    longword(view.camera,(unsigned)(camera*65536));
    for(unsigned i=0;i<32;++i){
        double depth=2.+i-(camera-std::floor(camera));
        word(view.road_y+i*2,(unsigned)std::min((double)view.bottom,view.horizon+80./depth));
        word(view.road_xy+i*4,0);word(view.road_xy+i*4+2,128);
    }
}
static void actor(int player,unsigned slot,unsigned id,int x,int y,unsigned scale){
    unsigned job=0xff7000+slot*22;
    longword(0xd346,player==1?0xff0a62:0xff0ee2);
    longword((id&65535)+4,(unsigned)((player==1?11.75:21.75)*65536));
    ram[((id&65535)+104)&65535]=0x40;
    word(job,x);word(job+2,y);word(job+4,scale);word(job+6,player==1?104:208);
    word(job+8,1);word(job+10,player==1?0x80:0xc0);word(job+18,0);
    rr_geometry_actor_begin(1,job,id);rr_geometry_actor(1,job,id,ram.data());
    rr_geometry_actor_end(1,job+22,ram.data());
    word(0xdc34,slot);longword(0x27a6,0xff8000);longword(0x27ce,0xff9000);word(0xce86,1);
    rr_geometry_job_begin(1,job,ram.data());
    unsigned sat=0x8000+slot*8;
    word(sat,128+y-8);word(sat+2,slot==3?0:slot+1);word(sat+4,0x8001+slot);word(sat+6,128+x-4);
    std::memset(ram.data()+0x9000+slot*32,(slot+1)*0x11,32);
    word(0xdc34,slot+1);rr_geometry_job_end(ram.data());
    std::memcpy(v.vram+0xc000+slot*8,ram.data()+sat,8);
    std::memcpy(v.vram+(slot+1)*32,ram.data()+0x9000+slot*32,32);
}
static void commit(unsigned frame,int top_x,unsigned top_scale){
    rr_geometry_frame(frame,3);
    actor(1,0,0xff0a62,100,94,12288);
    actor(1,1,0xff0f62,top_x,78,top_scale);
    actor(2,2,0xff0ee2,220,198,12288);
    // Same actor identity and image in both cameras, different coordinates.
    actor(2,3,0xff0f62,250,182,8192);
}
static double pixels(int line,int color){
    std::array<uint8_t,320> index{},opaque{},high{},owner{};
    GVDPSpriteLayer layer{line,320,0,index.data(),opaque.data(),high.data(),owner.data()};
    v.scanline=(uint16_t)line;double rect[4];sprites(nullptr,&v,0,rect);
    objects(nullptr,&v,&layer);
    double sum=0,count=0;
    for(int x=0;x<320;++x)if(opaque[x]&&index[x]==color){sum+=x;++count;}
    return count?sum/count:-1;
}
int main(){
    _putenv_s("RR_GEOMETRY","1");_putenv_s("RR_EDGE_RECOVERY","0");_putenv_s("RR_WIDE","0");
    std::vector<uint8_t> rom(0x100000);
    rr_geometry_source(1,rom.data(),(unsigned)rom.size(),ram.data());
    v.reg[5]=0x60;v.reg[12]=1;
    road(rr::race_view(1,1),10.);road(rr::race_view(1,2),20.);
    commit(10,70,8192);rr_geometry_frame(11,3);
    CHECK(sprites&&objects&&scroll&&viewport);
    int clip[2];CHECK(viewport(nullptr,&v,80,clip)&&clip[0]==0&&clip[1]==208);
    CHECK(viewport(nullptr,&v,180,clip)&&clip[0]==112&&clip[1]==320);
    CHECK(!viewport(nullptr,&v,108,clip));
    double upper=pixels(74,2),lower=pixels(178,4);
    CHECK(upper>=65&&upper<=75&&lower>=245&&lower<=255);
    CHECK(pixels(74,4)<0&&pixels(178,2)<0);
    commit(14,78,8800);rr_geometry_frame(15,3);
    double moved=pixels(74,2),still=pixels(178,4);
    CHECK(moved>upper+2&&std::abs(still-lower)<.01);
    // Each controlled player's complete native pose remains unpredicted.
    v.scanline=80;double rect[4];CHECK(sprites(nullptr,&v,0,rect));
    CHECK(rect[0]==96&&rect[1]==86&&rect[2]==104&&rect[3]==94);
    v.scanline=180;CHECK(sprites(nullptr,&v,2,rect));
    CHECK(rect[0]==216&&rect[1]==190&&rect[2]==224&&rect[3]==198);
    rr_geometry_frame(16,4);CHECK(!sprites&&!objects&&!scroll&&viewport);
    rr_geometry_frame(17,3);CHECK(pixels(74,2)<0&&pixels(178,4)<0);
    rr_geometry_frame(18,1);CHECK(!viewport);
    v.scanline=180;CHECK(!sprites(nullptr,&v,3,rect));
    _putenv_s("RR_WIDE","1");rr_geometry_frame(19,3);
    CHECK(viewport(nullptr,&v,80,clip)&&clip[0]==-54&&clip[1]==208);
    CHECK(viewport(nullptr,&v,180,clip)&&clip[0]==112&&clip[1]==374);
    // RR3 has different retained native camera/road addresses, same ownership.
    auto rr3=rr::race_view(2,2);
    CHECK(rr3.player==0xff0e32&&rr3.camera==0x094a&&rr3.road_y==0x3fa4&&rr3.road_xy==0x1faa);
    CHECK(!rr::race_view(0,2).id);
    std::puts("Split screen: independent actor/camera histories, current players, HUD clips, pause/mode resets passed");
}
