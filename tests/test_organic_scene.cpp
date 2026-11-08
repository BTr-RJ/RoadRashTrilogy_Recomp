#include "sprite_geometry.h"
#include "sprite_assets.h"
#include "road_surface.h"
#include "road_markings.h"
extern "C" {
#include "genesis_vdp.h"
}
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static GVDPVisualSprite sprite;
static GVDPHostSprites objects;
static GVDPVisualSurface material;
static GVDPVisualScroll scroll;
static GVDPVisualAudit audit;
extern "C" void gvdp_set_visual_sprites(GVDPVisualSprite fn,void*){sprite=fn;}
extern "C" void gvdp_set_visual_scroll(GVDPVisualScroll fn,void*){scroll=fn;}
extern "C" void gvdp_set_visual_objects(GVDPHostSprites fn,void*){objects=fn;}
extern "C" void gvdp_set_visual_surface(GVDPVisualSurface fn,void*){material=fn;}
extern "C" void gvdp_set_visual_audit(GVDPVisualAudit fn,void*){audit=fn;}
extern "C" void gvdp_set_visual_viewport(GVDPVisualViewport,void*){}
extern "C" const uint8_t *gvdp_sprite_patterns(unsigned,unsigned *bytes){*bytes=0;return nullptr;}
extern "C" int gvdp_sprite_pattern_line(unsigned,unsigned){return 0;}
extern "C" const uint32_t *gvdp_host_palette(void){return nullptr;}
static std::array<uint8_t,65536> ram;
static GVDP v;
static void word(unsigned at,unsigned value){ram[at]=(uint8_t)(value>>8);ram[at+1]=(uint8_t)value;}
static void longword(unsigned at,unsigned value){word(at,value>>16);word(at+2,value);}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"organic line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    // Odd-width nibble rows contain a guard byte, not a fifth pixel.
    std::vector<uint8_t> tiny(64);tiny[3]=8;
    const uint8_t art[]={1,2,3,2,0x60,0,0x12,0x30,0xff,0x45,0x60,0xee};
    std::memcpy(tiny.data()+8,art,sizeof art);
    auto decoded=rr::decode_sprite(tiny.data(),tiny.size(),0,0,true);
    CHECK(decoded.width==3&&decoded.height==2&&decoded.anchor_x==1&&decoded.anchor_y==2);
    CHECK(decoded.palette==3&&decoded.pixels==std::vector<uint8_t>({1,2,3,4,5,6}));
    CHECK(!rr::decode_sprite(tiny.data(),18,0,0,true));
    const uint8_t plain[]={3,2,0x60,0,0x12,0x30,0xff,0x45,0x60,0xee};
    std::memcpy(tiny.data()+8,plain,sizeof plain);
    decoded=rr::decode_sprite(tiny.data(),tiny.size(),0,0,false);
    CHECK(decoded.anchor_x==1&&decoded.anchor_y==2); // center/bottom, never top-left
    CHECK(decoded.pixels==std::vector<uint8_t>({1,2,3,4,5,6}));

    std::array<int,152> edge{};
    for(unsigned y=0;y<152;++y)edge[y]=std::max(320,640-(int)y*3);
    CHECK(rr::extend_clipped_edge(edge,140,320)==220);
    CHECK(rr::extend_clipped_edge(edge,100,320)==340);
    edge[103]=500;CHECK(rr::extend_clipped_edge(edge,140,320)==320);
    // In strong curves / off-road views the native clamp starts ABOVE line 72.
    // Continue at the first clamped row as well as the bottom, on both sides.
    for(unsigned y=0;y<152;++y)edge[y]=std::max(320,560-(int)y*5);
    CHECK(rr::extend_clipped_edge(edge,48,320)==320);
    CHECK(rr::extend_clipped_edge(edge,49,320)==315);
    CHECK(rr::extend_clipped_edge(edge,100,320)==60);
    for(unsigned y=0;y<152;++y)edge[y]=std::min(1056,480+(int)y*12);
    CHECK(rr::extend_clipped_edge(edge,49,1056,true)==1068);
    CHECK(rr::extend_clipped_edge(edge,80,1056,true)==1440);
    // The opposite edge must continue at an upper clamp as well. The road
    // center cannot be computed from one extended and one pinned half.
    for(unsigned y=0;y<152;++y)edge[y]=std::min(1056,744+(int)y*3);
    CHECK(rr::extend_clipped_edge(edge,140,1056,true)==1164);
    CHECK(rr::extend_clipped_edge(edge,90,1056,true)==1014);
    rr::RoadRays road;road.camera=10.5;
    for(unsigned i=0;i<32;++i)road.y[i]=64.+128./(2.+i-.5);
    double z,depth;CHECK(rr::road_depth(road,100.,z,depth));
    CHECK(std::abs(depth-128./36.)<1e-9&&std::abs(z-(road.camera+depth-2.))<1e-9);
    double far_z,far_depth;CHECK(rr::road_depth(road,80.,far_z,far_depth));CHECK(far_z>z);
    CHECK(!rr::road_depth(road,40.,z,depth));
    double py,clip;CHECK(rr::road_point(road,128./36.,py,clip));CHECK(std::abs(py-100.)<1e-9);
    for(auto& y:road.y)y+=18;
    CHECK(rr::road_point(road,128./36.,py,clip));CHECK(std::abs(py-118.)<1e-9);
    road.y[0]=152;road.y[1]=120;road.y[2]=90;road.y[3]=74;road.y[4]=82;road.y[5]=105;
    CHECK(rr::road_point(road,6.5,py,clip));CHECK(py==105&&clip==74);
    road.y[0]=road.y[1]=road.y[2]=152;road.y[3]=100;
    CHECK(rr::road_depth(road,150.,z,depth));
    CHECK(std::abs(depth-1./((50./52.)/3.5+(2./52.)/4.5))<1e-9);
    road.has_x=true;road.camera=10.5;road.horizon=76;
    for(unsigned i=0;i<32;++i){road.x[i]=i*128.-64;road.world_y[i]=200;}
    double wx;CHECK(rr::road_lateral(road,4.5,wx)&&wx==320);
    CHECK(rr::road_vehicle_height(road,2,0,py)&&py==176); // unclipped, not flattened at the HUD
    CHECK(rr::road_vehicle_height(road,.25,0,py)&&py==876); // finite near-camera continuation
    CHECK(!rr::road_vehicle_height(road,.249,0,py));
    // Actual road positions use relative Z+1 while sprite scale uses Z+2.
    // Reusing the artwork divisor moves a near foot by tens of pixels.
    road.position_bias=1;road.camera=10.5;road.has_height=true;
    for(unsigned i=0;i<32;++i){road.world_y[i]=93;road.y[i]=76+93./(1+i-.5);}
    CHECK(rr::road_vehicle_height(road,2.5,0,py)&&py==138);
    CHECK(rr::road_point(road,2.5,py,clip)&&std::abs(py-138)<1e-9);
    CHECK(rr::road_depth(road,138,z,depth)&&std::abs(depth-2.5)<1e-9);
    CHECK(rr::road_vehicle_height(road,1,0,py)&&py==448); // finite near pass
    auto previous=road,current=road;previous.camera=9.5;current.camera=10;
    for(int i=0;i<32;++i){previous.world_y[i]=100+5*i;previous.x[i]=i*i;
        current.world_y[i]=100+5*(i+1)-8;current.x[i]=(i+1)*(i+1)+6+2*i;}
    auto projected=rr::road_camera_basis(current,previous,10.25);
    CHECK(projected.world_y[12]==current.world_y[12]-4);
    CHECK(std::abs(projected.x[12]-(current.x[12]+15))<1e-9);
    CHECK(projected.y==current.y&&projected.clip==current.clip&&projected.camera==current.camera);
    // An incomplete/nonrigid road update cannot invent a camera translation.
    for(int i=0;i<32;++i)current.world_y[i]+=i*4;
    CHECK(rr::road_camera_basis(current,previous,10.25).world_y==current.world_y);

    _putenv_s("RR_GEOMETRY","1");_putenv_s("RR_ORGANIC","1");_putenv_s("RR_TRAFFIC_ZOOM","0");
    std::vector<uint8_t> rom(0x3c000);rom[0x3bf21]=0x80; // image 1 -> $80
    rom[0x80]=4;rom[0x81]=8;rom[0x82]=8;rom[0x83]=8;
    for(int y=0;y<8;++y)for(int x=0;x<4;++x)rom[0x86+y*5+x]=(uint8_t)((y+1)*17);
    rr_geometry_source(0,rom.data(),(unsigned)rom.size(),ram.data());rr_geometry_frame(0,1);
    word(0x7000,(unsigned)(uint16_t)-352);word(0x7002,100);word(0x7004,16384);
    word(0x7006,152);word(0x7008,1);word(0x700a,0x4080);word(0x700e,0);
    word(0xde64,0);longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());
    word(0x8000,220);word(0x8002,0);word(0x8004,0xa001);word(0x8006,284);
    std::memset(ram.data()+0x9000,0x11,32);word(0xde64,1);rr_geometry_job_end(ram.data());
    v.reg[5]=0x60;v.reg[12]=1;v.scanline=92;
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);std::memcpy(v.vram+32,ram.data()+0x9000,32);
    double bounds[4];CHECK(sprite&&objects&&material&&sprite(nullptr,&v,0,bounds));
    CHECK(bounds[0]==bounds[2]); // suppress exactly the matched native object
    std::array<uint8_t,320> indices{},opaque{},high{};
    GVDPSpriteLayer layer={92,320,0,indices.data(),opaque.data(),high.data()};
    objects(nullptr,&v,&layer);
    CHECK(indices[156]==17&&indices[163]==17&&!opaque[155]); // live committed palette
    // Descriptor $4000 must not turn the source art upside down.
    layer.line=99;opaque.fill(0);objects(nullptr,&v,&layer);CHECK(indices[156]==24);
    // One observation cannot establish an edge trajectory. Retaining it
    // would leave a stationary ghost after the native object disappeared.
    rr_geometry_frame(1,1);v.vram[0xc001]=0;
    CHECK(sprite(nullptr,&v,0,bounds)&&bounds[1]==-128); // native fallback, no stale art
    layer.line=92;opaque.fill(0);objects(nullptr,&v,&layer);CHECK(!opaque[156]);
    // With two confirmed poses, continue the full art after native culling.
    rr_geometry_frame(2,1);word(0x7004,20480);word(0xde64,0);
    rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);
    rr_geometry_job_end(ram.data());v.vram[0xc001]=220;
    CHECK(sprite(nullptr,&v,0,bounds));
    opaque.fill(0);objects(nullptr,&v,&layer);
    CHECK(opaque[155]&&opaque[164]); // newest committed scale appears on THIS frame
    rr_geometry_frame(3,1);v.vram[0xc001]=0;CHECK(sprite(nullptr,&v,0,bounds));
    layer.line=92;opaque.fill(0);objects(nullptr,&v,&layer);CHECK(opaque[156]);
    layer.line=152;opaque.fill(0);objects(nullptr,&v,&layer);CHECK(!opaque[156]);
    rr_geometry_frame(4,0);CHECK(!sprite&&!objects&&!material);
    rr_geometry_frame(5,1);CHECK(sprite(nullptr,&v,0,bounds));
    layer.line=92;opaque.fill(0);objects(nullptr,&v,&layer);CHECK(!opaque[156]);
    // VRAM keeps stale SAT entries after link zero. A perfect attribute and
    // tile match in an unlinked slot must never resurrect that object.
    rr_geometry_frame(6,1);word(0xde64,1);
    std::memcpy(ram.data()+0x8008,ram.data()+0x8000,8);
    rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,2);
    rr_geometry_job_end(ram.data());
    std::memcpy(v.vram+0xc008,ram.data()+0x8008,8);
    CHECK(!sprite(nullptr,&v,1,bounds));
    opaque.fill(0);objects(nullptr,&v,&layer);CHECK(!opaque[156]);
    // Reusing a former scenery descriptor for a rider must preserve the
    // committed animation/palette pieces, without drawing a second body.
    rr_geometry_frame(7,1);word(0x7008,2);word(0x7004,16384);word(0xde64,0);
    rr_geometry_project(0,0xff7000,0xff2100,65536,0,0);
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);
    rr_geometry_job_end(ram.data());v.vram[0xc001]=220;
    CHECK(sprite(nullptr,&v,0,bounds));
    CHECK(bounds[0]==156&&bounds[1]==92&&bounds[2]==164&&bounds[3]==100);
    opaque.fill(0);objects(nullptr,&v,&layer);CHECK(!opaque[156]);
    // A new attack/vehicle image may use a different native anchor/scale.
    // Do not extrapolate that pose from the preceding image's geometry.
    rr_geometry_frame(11,1);word(0x7008,3);word(0x7004,12288);word(0xde64,0);
    rr_geometry_project(0,0xff7000,0xff2100,65536,0,0);
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));
    rr_geometry_frame(12,1);CHECK(sprite(nullptr,&v,0,bounds));
    CHECK(bounds[2]-bounds[0]==8&&bounds[3]-bounds[1]==8);
    // Keep a multi-job actor as one current native assembly. Each piece
    // retains its palette and the original first-wins SAT overlap order.
    rr_geometry_reset();rr_geometry_frame(13,1);
    word(0x7004,8192);word(0x7008,3);word(0x7014,(uint16_t)-352);word(0x7016,100);
    word(0x7018,8192);word(0x701a,152);word(0x701c,4);word(0x701e,0x4080);word(0x7026,0);
    rr_geometry_actor_begin(0,0xff7000,0xff2200);rr_geometry_actor_end(0,0xff7028,ram.data());
    word(0x8002,1);word(0x8008,220);word(0x800a,0);word(0x800c,0xc002);word(0x800e,288);
    std::memset(ram.data()+0x9020,0x22,32);std::memcpy(v.vram+64,ram.data()+0x9020,32);
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    rr_geometry_job_begin(0,0xff7014,ram.data());word(0xde64,2);rr_geometry_job_end(ram.data());
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,16);
    CHECK(sprite(nullptr,&v,0,bounds)&&bounds[0]==bounds[2]);
    std::array<uint8_t,320> owners{};layer.owner=owners.data();layer.line=92;opaque.fill(0);
    objects(nullptr,&v,&layer);CHECK(indices[156]==17&&indices[163]==17&&indices[164]==34&&indices[167]==34);
    opaque.fill(0);owners.fill(0);opaque[157]=opaque[166]=1;owners[157]=1;owners[166]=9;
    indices[157]=indices[166]=99;objects(nullptr,&v,&layer);CHECK(indices[157]==99&&indices[166]==34);
    // A partial upload must show the last COMPLETE assembly, never a mix of
    // old torso/new bike. Header ownership still suppresses the torn tiles.
    rr_geometry_frame(14,1);v.vram[32]=0xff;
    CHECK(sprite(nullptr,&v,0,bounds)&&bounds[0]==bounds[2]);
    opaque.fill(0);owners.fill(0);objects(nullptr,&v,&layer);CHECK(indices[156]==17&&indices[164]==34);
    rr_geometry_frame(15,1);v.vram[0xc001]=0;CHECK(sprite(nullptr,&v,1,bounds));
    opaque.fill(0);owners.fill(0);objects(nullptr,&v,&layer);CHECK(opaque[156]&&indices[156]==17);
    // Explicit native removal wins over the short retention window.
    rr_geometry_frame(16,1);rr_geometry_actor_begin(0,0xff7000,0xff2200);
    rr_geometry_actor_end(0,0xff7000,ram.data());v.vram[0xc003]=0;
    CHECK(sprite(nullptr,&v,0,bounds));opaque.fill(0);objects(nullptr,&v,&layer);CHECK(!opaque[156]);
    // Near-plane prediction must never turn a still-visible native actor
    // into a zero-sized rectangle for one frame.
    rr_geometry_frame(17,1);word(0x7004,12288);word(0x7018,12288);
    rr_geometry_actor_begin(0,0xff7000,0xff2200);rr_geometry_actor_end(0,0xff7028,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    rr_geometry_job_begin(0,0xff7014,ram.data());word(0xde64,2);rr_geometry_job_end(ram.data());
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,16);v.vram[32]=0x11;
    CHECK(sprite(nullptr,&v,0,bounds));rr_geometry_frame(23,1);CHECK(sprite(nullptr,&v,0,bounds));
    opaque.fill(0);owners.fill(0);objects(nullptr,&v,&layer);CHECK(opaque[156]&&indices[156]==17);
    // Raster geometry belongs to the current bitmap. An independent old V
    // scroll or delayed half-road width exposes command data as blue stripes.
    rr_geometry_reset();v.reg[11]=3;v.reg[13]=0x2c;
    rr_geometry_frame(40,1);int hs[4]={100,200,8,16};
    CHECK(scroll(nullptr,&v,80,hs));CHECK(hs[0]==100&&hs[2]==8);
    v.vram[0xb001]=1;rr_geometry_frame(44,1);
    int next[4]={120,220,24,32};CHECK(scroll(nullptr,&v,80,next));
    CHECK(next[0]==120&&next[1]==220&&next[2]==24&&next[3]==32);
    rr_geometry_frame(45,1);int predicted[4]={120,220,24,32};
    CHECK(scroll(nullptr,&v,80,predicted));
    CHECK(predicted[0]==124&&predicted[1]==224&&predicted[2]==24&&predicted[3]==32);
    // The same paint update contains two leaves far apart horizontally.
    // Replacing only the leaf near the inferred center duplicates the stripe.
    rr_geometry_reset();ram.fill(0);v=GVDP{};
    v.reg[2]=0x10;v.reg[4]=3;v.reg[5]=0x60;v.reg[7]=57;
    v.reg[11]=3;v.reg[13]=0x2c;v.reg[16]=3;
    v.cram[57]=0x444;v.cram[60]=0xee;
    rr::RoadRays paint_road;
    for(unsigned i=0;i<32;++i){paint_road.y[i]=152.-i*3;word(0xd77c+i*2,(unsigned)paint_road.y[i]);}
    auto vw=[&](unsigned at,unsigned value){v.vram[at]=(uint8_t)(value>>8);v.vram[at+1]=(uint8_t)value;};
    for(int y=0;y<152;++y){vw(0xb000+y*4,500);vw(0xb002+y*4,(unsigned)(uint16_t)-180);}
    rr::MarkingModel paint;paint.period=2;paint.phase=.375;
    unsigned far_slot=11;
    for(unsigned slot=0;slot<=far_slot;++slot){
        unsigned group=slot==far_slot?5:slot,y0=64+group*8,tile=16+group;
        unsigned at=0xc000+slot*8;
        vw(at,128+y0);vw(at+2,slot==far_slot?0:slot+1);
        vw(at+4,0x6000|tile);vw(at+6,128+(slot==far_slot?205:160));
        for(unsigned row=0;row<8;++row){double z,d;
            bool on=rr::road_depth(paint_road,y0+row+.5,z,d)&&rr::marking_at(paint,z);
            v.vram[tile*32+row*4]=on?0xcc:0;
            v.vram[tile*32+row*4+1]=on?0xcc:0;
        }
    }
    rr_geometry_frame(60,1);
    for(unsigned y=64;y<152;++y){v.scanline=y;int values[4]={500,844,0,0};CHECK(scroll(nullptr,&v,y,values));}
    rr_geometry_frame(61,1);v.scanline=104;int paint_scroll[4]={500,844,0,0};
    CHECK(scroll(nullptr,&v,104,paint_scroll));
    CHECK(sprite(nullptr,&v,5,bounds)&&bounds[0]==bounds[2]);
    CHECK(sprite(nullptr,&v,far_slot,bounds)&&bounds[0]==bounds[2]);
    CHECK(material(nullptr,&v,104,205,57)==57); // no second synthetic stripe
    v.reg[7]=0;CHECK(scroll(nullptr,&v,104,paint_scroll));
    CHECK(sprite(nullptr,&v,far_slot,bounds)&&bounds[2]>bounds[0]); // row fallback is atomic
    // A single scenery job can commit several copies of the same full object.
    // Replacing all four SAT pieces with one billboard deletes the second tree.
    rr_geometry_reset();ram.fill(0);v=GVDP{};
    v.reg[5]=0x60;v.reg[12]=1;v.scanline=92;
    word(0x7000,(unsigned)(uint16_t)-352);word(0x7002,100);word(0x7004,16384);
    word(0x7006,152);word(0x7008,1);word(0x700a,0x4080);
    word(0x7012,2);
    rom[0x3bf24]=1;rom[0x100]=4;rom[0x101]=16;rom[0x102]=rom[0x103]=8;
    for(int y=0;y<8;++y)for(int x=0;x<4;++x)rom[0x106+y*5+x]=0x22;
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x8000,220);word(0x8002,1);word(0x8004,0xa001);word(0x8006,284);
    word(0x8008,212);word(0x800a,2);word(0x800c,0xa002);word(0x800e,284);
    word(0x8010,220);word(0x8012,3);word(0x8014,0xc801);word(0x8016,332);
    word(0x8018,212);word(0x801a,0);word(0x801c,0xc802);word(0x801e,332);
    std::memset(ram.data()+0x9000,0x11,32);std::memset(v.vram+32,0x11,32);
    std::memset(ram.data()+0x9020,0x22,32);std::memset(v.vram+64,0x22,32);
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,32);rr_geometry_frame(60,1);
    rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,4);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds)&&bounds[0]==bounds[2]);
    opaque.fill(0);owners.fill(0);layer.line=92;objects(nullptr,&v,&layer);
    CHECK(opaque[156]&&indices[156]==17&&opaque[204]&&indices[204]==33);
    opaque.fill(0);owners.fill(0);layer.line=84;objects(nullptr,&v,&layer);
    CHECK(opaque[156]&&indices[156]==18&&opaque[204]&&indices[204]==34);
    // Too-small source art must never suppress a still-visible native pixel.
    rr_geometry_reset();rr_geometry_frame(61,1);word(0x7004,1024);word(0xde64,0);
    rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,4);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds)&&bounds[2]>bounds[0]);
    rr_geometry_enable_coverage(1);
    // A nominally visible but completely transparent source replacement must
    // leave the committed native assembly visible, too.
    rr_geometry_reset();rr_geometry_frame(62,1);word(0x7004,16384);word(0xde64,0);
    rom[0x3bf29]=0xc0;rom[0xc0]=4;rom[0xc1]=8;rom[0xc2]=rom[0xc3]=8;
    word(0x7008,3);word(0x7012,0);
    rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,4);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds)&&bounds[2]>bounds[0]);
    rr_geometry_enable_coverage(0);
    // Distant, subpixel world grass must retain the authored raster stipple.
    // Replacing the minor shade with the dominant color erases the whole field.
    rr_geometry_reset();ram.fill(0);v=GVDP{};
    v.reg[2]=0x10;v.reg[16]=3;
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,152-i*3);
    for(unsigned y=14;y<=18;++y)vw(0x4000+(y*128+60)*2,0x6004);
    std::memset(v.vram+128,0xaa,32);
    for(unsigned y=0;y<8;++y)v.vram[128+y*4]=0xba;
    rr_geometry_frame(70,1);
    for(int x=-53;x<373;++x)CHECK(material(nullptr,&v,80,x,59)==59);
    CHECK(material(nullptr,&v,80,100,57)==57); // asphalt stays out of the material
    // Pause freezes animation, not the finite road's spatial continuation.
    _putenv_s("RR_WIDE","1");v.reg[11]=3;v.reg[13]=0x2c;
    for(unsigned y=0;y<152;++y){vw(0xb000+y*4,std::max(320,640-(int)y*3));vw(0xb002+y*4,(unsigned)(uint16_t)-180);}
    rr_geometry_frame(80,2);CHECK(!sprite&&!objects&&!material&&scroll);
    int frozen[4]={320,844,8,16};CHECK(scroll(nullptr,&v,140,frozen));
    CHECK(frozen[0]==220&&frozen[1]==844&&frozen[2]==8&&frozen[3]==16);
    rr_geometry_frame(81,0);CHECK(!scroll); // menus never inherit a road correction
    // Uncropped source is captured before the guest's 320px software cull.
    // Preserve native pixels in the center and recover the missing left half.
    _putenv_s("RR_ARCADE_MOTION","0"); // isolate recovery from motion prediction
    rr_geometry_reset();ram.fill(0);v=GVDP{};v.reg[5]=0x60;v.reg[12]=1;v.scanline=92;
    rom[0x3bf2c]=1;rom[0x3bf2d]=0x40;rom[0x140]=8;rom[0x141]=8;rom[0x142]=16;rom[0x143]=8;rom[0x144]=0x20;
    for(int y=0;y<8;++y)for(int x=0;x<8;++x)rom[0x146+y*9+x]=x<4?0x11:0x22;
    word(0x7000,0);word(0x7002,100);word(0x7004,16384);word(0x7006,224);word(0x7008,4);word(0x700a,0x80);
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x8000,220);word(0x8002,0);word(0x8004,0xa001);word(0x8006,128);
    std::memset(ram.data()+0x9000,0x22,32);std::memset(v.vram+32,0x22,32);
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);rr_geometry_frame(90,1);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    word(0x7000,(unsigned)(uint16_t)-512);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));
    std::array<uint8_t,428> edge_index{},edge_op{},edge_high{},edge_owner{};
    GVDPSpriteLayer wide_layer={92,428,54,edge_index.data(),edge_op.data(),edge_high.data(),edge_owner.data()};
    objects(nullptr,&v,&wide_layer);CHECK(edge_op[46]&&edge_index[46]==17&&edge_index[54]==18);
    // A guest-cull is different from actual actor removal: fresh projected
    // source still exists, even when the native job count becomes zero.
    rr_geometry_frame(91,1);v.vram[0xc001]=0;word(0x7000,(unsigned)(uint16_t)-16);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));edge_op.fill(0);objects(nullptr,&v,&wide_layer);
    CHECK(edge_op[30]&&edge_index[30]==17&&edge_index[38]==18);
    rr_geometry_frame(92,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));edge_op.fill(0);objects(nullptr,&v,&wide_layer);
    CHECK(std::none_of(edge_op.begin(),edge_op.end(),[](uint8_t x){return x!=0;}));
    // The opposite edge recovers the right half beyond native column 319.
    rr_geometry_reset();rr_geometry_frame(93,1);word(0x7000,320);word(0xde64,0);
    word(0x8000,220);word(0x8006,440);std::memset(ram.data()+0x9000,0x11,32);std::memset(v.vram+32,0x11,32);
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));edge_op.fill(0);objects(nullptr,&v,&wide_layer);
    CHECK(edge_index[366]==17&&edge_op[374]&&edge_index[374]==18);
    // Some guest paths cull before entering the actor builder at all. Seal
    // the complete pre-cull packet at the next frame, then use the wide view.
    rr_geometry_frame(94,1);word(0x7000,336);v.vram[0xc001]=0;
    rr_geometry_actor(0,0xff7000,0xff2300,ram.data());
    rr_geometry_frame(95,1);CHECK(sprite(nullptr,&v,0,bounds));
    edge_op.fill(0);objects(nullptr,&v,&wide_layer);CHECK(edge_op[382]&&edge_index[382]==17&&edge_index[390]==18);
    // Actor depth remains continuous across animation changes. Newly committed
    // poses appear immediately; only gaps use prediction, with no replay lag.
    _putenv_s("RR_ARCADE_MOTION","1");
    rr_geometry_reset();rr_geometry_frame(200,1);word(0x7000,160);word(0x7002,100);
    word(0x7004,16384);word(0x7008,4);v.vram[0xc001]=0;v.scanline=95;wide_layer.line=95;
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));
    auto extent=[&](){edge_op.fill(0);edge_index.fill(0);objects(nullptr,&v,&wide_layer);
        return (int)std::count(edge_op.begin(),edge_op.end(),(uint8_t)1);};
    int first_width=extent();CHECK(first_width>0);
    // Same art bounds, different palette/image: animation must change now.
    rom[0x3bf34]=1;rom[0x3bf35]=0xc0;
    std::memcpy(rom.data()+0x1c0,rom.data()+0x140,78);rom[0x1c4]=0x40;
    rr_geometry_frame(204,1);word(0x7004,20480);word(0x7008,6);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));int committed_width=extent();CHECK(committed_width>first_width);
    CHECK(edge_index[214]==34); // latest image, not a buffered old palette
    int predicted_width=committed_width;
    for(unsigned frame=205;frame<=207;++frame){rr_geometry_frame(frame,1);CHECK(sprite(nullptr,&v,0,bounds));
        int width=extent();CHECK(width>=predicted_width);predicted_width=width;}
    CHECK(predicted_width>committed_width); // motion during native update gaps
    rr_geometry_frame(208,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0); // explicit removal wins
    // Crossing the old 2x bitmap-size guard used to shrink a near sprite back
    // to native size. Depth prediction must stay finite and monotonic here.
    rr_geometry_reset();wide_layer.line=60;v.scanline=60;word(0x7002,64);
    for(unsigned frame:{210u,214u}){rr_geometry_frame(frame,1);word(0x7004,frame==210?8192:16384);
        rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
        CHECK(sprite(nullptr,&v,0,bounds));}
    int near_width=extent();CHECK(near_width>0);
    for(unsigned frame=215;frame<=220;++frame){rr_geometry_frame(frame,1);CHECK(sprite(nullptr,&v,0,bounds));
        int width=extent();CHECK(width>=near_width);near_width=width;}
    CHECK(near_width>32);
    // A qualified source pose must sample original detail at the NEW scale,
    // rather than enlarge the already minified all-blue native bitmap.
    rr_geometry_enable_coverage(1);rr_geometry_reset();rr_geometry_frame(230,1);word(0x7004,4096);word(0x7008,7);
    rom[0x3bf38]=2;rom[0x3bf39]=0x40;std::memcpy(rom.data()+0x240,rom.data()+0x140,78);
    for(int y=0;y<8;++y)for(int x=0;x<8;++x)rom[0x246+y*9+x]=0x12;
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));rr_geometry_frame(234,1);word(0x7004,8192);
    word(0x8000,184);word(0x8002,0);word(0x8004,0xa001);word(0x8006,284);
    std::memset(ram.data()+0x9000,0,16);std::memset(ram.data()+0x9010,0x22,16);
    std::memcpy(v.vram+32,ram.data()+0x9000,32);std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==8);
    rr_geometry_frame(235,1);CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>8);
    CHECK(std::find(edge_index.begin()+200,edge_index.begin()+228,(uint8_t)17)!=edge_index.begin()+228);
    rr_geometry_frame(236,1);std::memset(ram.data()+0x9010,0x33,16);std::memcpy(v.vram+32,ram.data()+0x9000,32);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==8);CHECK(edge_index[214]==19); // unsupported recolor stays native
    // Exercise the actual audit -> RGBA overlay path, with a native foreground
    // owner and the HUD outside the race rectangle. This also uses the rejected
    // recolor above, so a wrong ROM color must not leak through the new resolver.
    CHECK(audit);v.reg[1]=0x40;v.cram[19]=0x000e;
    std::array<uint8_t,426> bg{},own{},hi{};std::array<uint32_t,426> rgba;
    rgba.fill(0xff123456);GVDPVisualAuditRow row{};row.line=wide_layer.line;row.total=426;row.offset=53;
    row.background_index=bg.data();row.background_owner=own.data();row.plane_high=hi.data();
    audit(nullptr,&v,&row);rr_geometry_resolve(&v,row.line,rgba.data(),426);
    CHECK(std::find(rgba.begin(),rgba.end(),0xffff0000)!=rgba.end());
    row.total=320;row.offset=0;rgba.fill(0xff123456);
    audit(nullptr,&v,&row);rr_geometry_resolve(&v,row.line,rgba.data(),320);
    CHECK(std::find(rgba.begin(),rgba.begin()+320,0xffff0000)!=rgba.begin()+320);
    CHECK(rgba[320]==0xff123456);row.total=426;row.offset=53;
    own.fill(1);rgba.fill(0xff123456);audit(nullptr,&v,&row);rr_geometry_resolve(&v,row.line,rgba.data(),426);
    CHECK(std::all_of(rgba.begin(),rgba.end(),[](uint32_t c){return c==0xff123456;}));
    own.fill(0);row.line=152;rgba.fill(0xff123456);audit(nullptr,&v,&row);rr_geometry_resolve(&v,row.line,rgba.data(),426);
    CHECK(std::all_of(rgba.begin(),rgba.end(),[](uint32_t c){return c==0xff123456;}));
    rr_geometry_enable_coverage(0);
    // Real vehicle depth calibration: follow the current hill, preserve the
    // player's size reference, and move the crest mask independently of zoom.
    rr_geometry_reset();ram.fill(0);v=GVDP{};v.reg[5]=0x60;v.reg[12]=1;
    longword(0x1e68,10u<<16);word(0x7000,160);word(0x7008,4);word(0x700a,0x80);word(0x7006,224);
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,64+128/(2+i));
    auto projected_actor=[&](unsigned frame,unsigned world,unsigned scale,unsigned y){
        longword(0x2304,world);word(0x7004,scale);word(0x7002,y);rr_geometry_frame(frame,1);
        rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
        v.scanline=95;sprite(nullptr,&v,0,bounds);
    };
    projected_actor(300,12u<<16,10240,96);projected_actor(304,(11u<<16)+49152,10923,98);
    auto center=[&](){double sum=0,count=0;for(unsigned i=0;i<edge_op.size();++i)if(edge_op[i]){sum+=i;++count;}return count?sum/count:-1;};
    for(unsigned i=0;i<32;++i)word(0x310a+i*4,128);
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,82+128/(2+i));
    rr_geometry_frame(305,1);v.scanline=114;wide_layer.line=114;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    CHECK(center()>245); // road steering is not vehicle lane velocity
    wide_layer.line=96;CHECK(extent()==0); // no fixed-horizon ghost below the crest
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,52+128/(2+i));
    for(unsigned i=0;i<32;++i)word(0x310a+i*4,(unsigned)(uint16_t)-128);
    rr_geometry_frame(306,1);v.scanline=84;wide_layer.line=84;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    CHECK(center()<185);
    wide_layer.line=114;CHECK(extent()==0); // follow the opposite grade as well
    for(unsigned i=0;i<32;++i)word(0x310a+i*4,0);
    rr_geometry_frame(307,1);v.scanline=84;wide_layer.line=84;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    CHECK(center()>210&&center()<218); // horizontal-only changes must update the road snapshot
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,82+128/(2+i));
    projected_actor(308,(10u<<16)+32768,16384,133);wide_layer.line=128;CHECK(extent()==16);
    rr_geometry_frame(309,1);v.scanline=140;wide_layer.line=140;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);
    rr_geometry_frame(310,1);v.scanline=149;wide_layer.line=149;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);
    // The camera-sized rider exits below the viewport without inflating.
    rr_geometry_frame(311,1);v.scanline=151;wide_layer.line=151;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    // Empty builder packets may arrive before the old SAT upload disappears.
    // Keep that still-present pose on the current road; never jump backwards
    // to its old bitmap anchor, and never retain it after the SAT is removed.
    longword(0x2304,(10u<<16)+32768);word(0x7004,16384);word(0x7002,133);
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x8000,253);word(0x8002,4<<8);word(0x8004,0xa001);word(0x8006,280);
    std::memset(ram.data()+0x9000,0x11,32);std::memset(ram.data()+0x9020,0x22,32);
    std::memcpy(v.vram+32,ram.data()+0x9000,64);std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
    rr_geometry_frame(312,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    v.scanline=128;wide_layer.line=128;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);
    rr_geometry_frame(313,1);longword(0x2304,(10u<<16)+16384);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    v.scanline=136;wide_layer.line=136;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);
    wide_layer.line=128;CHECK(extent()==0);
    rr_geometry_frame(314,1);v.vram[0xc000]=0;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    rr_geometry_reset();
    rom[0x3bf3c]=2;rom[0x3bf3d]=0xa0;rom[0x2a0]=8;rom[0x2a1]=40;rom[0x2a2]=16;rom[0x2a3]=40;rom[0x2a4]=0x20;
    for(int y=0;y<40;++y)for(int x=0;x<8;++x)rom[0x2a6+y*9+x]=0x11;
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,100+i*3);
    word(0xd77c,146);word(0xd77e,126);word(0xd780,100);word(0xd782,90);word(0xd784,96);
    word(0x7008,8);word(0x7006,90);
    projected_actor(316,16u<<16,5120,118);projected_actor(320,14u<<16,6827,96);
    rr_geometry_frame(321,1);v.scanline=89;wide_layer.line=89;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    wide_layer.line=90;CHECK(extent()==0); // real crest stays at y=90, regardless of scale
    // A live far vehicle crosses the native draw cutoff without blinking.
    // Deactivation, excessive depth and stale evidence each end this hold.
    rr_geometry_reset();ram[0x2368]=0x40;word(0x7006,152);
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,64+128/(2+i));
    projected_actor(330,40u<<16,1280,68);wide_layer.line=66;CHECK(extent()>0);
    rr_geometry_frame(331,1);longword(0x2304,(40u<<16)+16384);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    v.scanline=66;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    ram[0x2368]=0;rr_geometry_frame(332,1);CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    ram[0x2368]=0x40;rr_geometry_frame(333,1);longword(0x2304,45u<<16);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    rr_geometry_frame(334,1);longword(0x2304,(40u<<16)+16384);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    rr_geometry_frame(347,1);CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    // Main-camera and rear-view mirror descriptors share an actor ID. Keep
    // forward artwork growing through the short pass, then exit below the viewport;
    // never reinterpret the two HUD copies as a giant forward assembly.
    rr_geometry_reset();word(0x7006,224);
    for(unsigned i=0;i<32;++i){word(0x310a+i*4,0);word(0x310c+i*4,80);
        word(0xd77c+i*2,std::min(152u,76+80/(1+i)));}
    projected_actor(350,14u<<16,6827,92);wide_layer.line=90;CHECK(extent()>0);
    projected_actor(354,(10u<<16)+32768,16384,129);wide_layer.line=125;
    int player_plane_width=extent();CHECK(player_plane_width==16);
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x8000,217);word(0x8002,(7<<8)|1);word(0x8004,0xa001);word(0x8006,280);
    word(0x8008,249);word(0x800a,4<<8);word(0x800c,0xa009);word(0x800e,280);
    std::memset(ram.data()+0x9000,0x11,320);std::memcpy(v.vram+32,ram.data()+0x9000,320);
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,16);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,2);rr_geometry_job_end(ram.data());
    v.scanline=130;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    word(0x7008,7);projected_actor(356,(9u<<16)+58982,20480,200);
    word(0x8000,328);word(0x8002,4<<8);word(0x8004,0xa001);word(0x8006,160);
    std::memset(ram.data()+0x9000,0x33,64);std::memcpy(v.vram+32,ram.data()+0x9000,64);
    std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7014,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    v.scanline=145;CHECK(sprite(nullptr,&v,0,bounds));
    wide_layer.line=145;int pass_width=extent();CHECK(pass_width>player_plane_width);
    CHECK(edge_index[214]==17); // front art, not mirror art
    rr_geometry_frame(357,1);longword(0x2304,(9u<<16)+39322);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
    v.scanline=151;wide_layer.line=151;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>pass_width);
    int closer_width=extent();
    projected_actor(358,(9u<<16)+55706,20480,200);wide_layer.line=151;
    CHECK(extent()>=closer_width);CHECK(edge_index[214]==17); // braking cannot shrink/reintroduce mirror art
    auto visible_top=[&](){int top=152;unsigned saved_line=wide_layer.line;
        for(unsigned y=0;y<152;++y){wide_layer.line=y;if(extent()>0){top=(int)y;break;}}
        wide_layer.line=saved_line;return top;};
    int exit_top=visible_top();CHECK(exit_top<152);
    for(unsigned frame=359;frame<370;++frame){
        // A braking/grade correction moves the physical feet upwards while
        // the billboard keeps growing. Its complete top must still exit in
        // one direction, rather than expose a huge roof above the HUD again.
        longword(0x2304,(9u<<16)+32768);
        rr_geometry_frame(frame,1);
        rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor_end(0,0xff7000,ram.data());
        v.scanline=145;CHECK(sprite(nullptr,&v,0,bounds));int next_top=visible_top();CHECK(next_top>=exit_top);exit_top=next_top;
    }
    CHECK(exit_top==152);
    rr_geometry_frame(370,1);longword(0x2304,(8u<<16)+16384);
    rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7014,ram.data());
    v.scanline=145;wide_layer.line=145;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0); // no legacy fallback ghost after the pass
    v.scanline=200;CHECK(!sprite(nullptr,&v,0,bounds)); // HUD remains native
    // Forecast a lane only when both native intervals support its direction.
    // Reversing steering must not extrapolate past the latest measured lane.
    rr_geometry_reset();word(0x7008,4);word(0x700a,0x80);word(0x7006,152);
    longword(0x1e68,10u<<16);ram[0x2368]=0x40;
    for(unsigned i=0;i<32;++i){word(0x310a+i*4,0);word(0x310c+i*4,200);
        word(0xd77c+i*2,std::min(152u,76+200/(1+i)));}
    word(0x2330,(unsigned)(uint16_t)-40);word(0x7000,152);projected_actor(400,13u<<16,8192,116);
    word(0x2330,0);word(0x7000,160);projected_actor(404,13u<<16,8192,116);
    word(0x2330,(unsigned)(uint16_t)-40);word(0x7000,152);projected_actor(408,13u<<16,8192,116);
    rr_geometry_frame(409,1);v.scanline=114;wide_layer.line=114;CHECK(sprite(nullptr,&v,0,bounds));
    CHECK(extent()>0);CHECK(std::abs(center()-205.5)<.6);
    // Reusing the actor for a different projection (e.g. a crash/running
    // image) must invalidate the old riding calibration and ground anchor.
    word(0x7000,190);projected_actor(412,13u<<16,16384,100);
    v.scanline=95;wide_layer.line=95;CHECK(sprite(nullptr,&v,0,bounds));
    CHECK(extent()==16);CHECK(center()>240);
    // A detached rider and bike may share an actor, but have separate size
    // and placement. Camera prediction must not scale them as one rigid pose.
    longword(0x1e68,(9u<<16)+32768);word(0x7000,153);projected_actor(413,13u<<16,7447,112);
    longword(0x1e68,10u<<16);word(0x7000,152);word(0x7002,116);word(0x7004,8192);
    for(unsigned i=0;i<20;i+=2)word(0x7014+i,((unsigned)ram[0x7000+i]<<8)|ram[0x7001+i]);
    word(0x7014,210);word(0x7016,90);word(0x7018,16384);word(0x701c,6);
    rr_geometry_frame(414,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);
    rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor(0,0xff7014,0xff2300,ram.data());
    rr_geometry_actor_end(0,0xff7000,ram.data());
    v.scanline=85;wide_layer.line=85;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);
    rr_geometry_frame(415,1);CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);
    CHECK(center()>260&&center()<268);
    // Mid-distance builder gaps keep a verified live, unchanged vehicle.
    // A changed animation state, actual removal, a crest and stale evidence
    // must not be mistaken for a transient upload gap.
    word(0x7000,156);projected_actor(416,18u<<16,4096,98);wide_layer.line=96;
    CHECK(extent()>0);
    rr_geometry_frame(417,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    v.scanline=96;CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()>0);
    ram[0x236e]=1;rr_geometry_frame(418,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    ram[0x236e]=0;projected_actor(419,18u<<16,4096,98);CHECK(extent()>0);
    word(0xd77e,80);rr_geometry_frame(420,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);rr_geometry_actor_end(0,0xff7000,ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0); // current crest still masks the held pose
    rr_geometry_frame(432,1);CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==0);
    // Descriptor order can differ from native SAT order. Preserve an attack
    // overlay's front-to-back order in source sampling AND recovered margins.
    _putenv_s("RR_ARCADE_MOTION","0");
    rr_geometry_reset();ram.fill(0);v=GVDP{};v.reg[5]=0x60;v.reg[12]=1;v.scanline=95;wide_layer.line=95;
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x7000,320);word(0x7002,100);word(0x7004,16384);word(0x7006,152);word(0x7008,4);word(0x700a,0x80);
    for(unsigned i=0;i<20;i+=2)word(0x7014+i,((unsigned)ram[0x7000+i]<<8)|ram[0x7001+i]);
    word(0x701c,6);
    // First SAT entry is the overlay (palette 2); body is second (palette 1).
    word(0x8000,220);word(0x8002,1);word(0x8004,0xc001);word(0x8006,440);
    word(0x8008,220);word(0x800a,0);word(0x800c,0xa002);word(0x800e,440);
    std::memset(ram.data()+0x9000,0x22,32);std::memset(ram.data()+0x9020,0x11,32);
    std::memcpy(v.vram+32,ram.data()+0x9000,64);std::memcpy(v.vram+0xc000,ram.data()+0x8000,16);
    rr_geometry_frame(440,1);rr_geometry_actor_begin(0,0xff7000,0xff2300);
    rr_geometry_actor(0,0xff7000,0xff2300,ram.data());rr_geometry_actor(0,0xff7014,0xff2300,ram.data());
    rr_geometry_actor_end(0,0xff7028,ram.data());
    word(0xde64,0);rr_geometry_job_begin(0,0xff7014,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
    rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,2);rr_geometry_job_end(ram.data());
    CHECK(sprite(nullptr,&v,0,bounds));edge_op.fill(0);objects(nullptr,&v,&wide_layer);
    CHECK(edge_op[378]&&edge_index[378]==34); // recovered x=324 keeps the overlay in front
    _putenv_s("RR_ARCADE_MOTION","1");
    // The grid/close-combat path places the nearest rival differently from
    // the far road projection. A valid calibration must reproduce its actual
    // native anchor, rather than clamp its correction to eight pixels.
    rr_geometry_reset();ram.fill(0);v=GVDP{};v.reg[5]=0x60;v.reg[12]=1;
    longword(0x1e68,(7u<<16)+32768);word(0x2330,32);ram[0x2368]=0x40;
    for(unsigned i=0;i<32;++i){word(0x310a+i*4,(unsigned)(uint16_t)-120);word(0x310c+i*4,93);
        word(0xd77c+i*2,138-i*2);}
    word(0x7000,102);word(0x7008,4);word(0x700a,0x80);word(0x7006,152);
    projected_actor(450,8u<<16,16384,138);wide_layer.line=134;
    CHECK(extent()==16);CHECK(std::abs(center()-155.5)<.6);
    for(unsigned frame=451;frame<454;++frame){rr_geometry_frame(frame,1);v.scanline=134;
        CHECK(sprite(nullptr,&v,0,bounds));CHECK(extent()==16);CHECK(std::abs(center()-155.5)<.6);}
    word(0x7008,6);projected_actor(454,8u<<16,16384,138);
    CHECK(extent()==16);CHECK(std::abs(center()-155.5)<.6);CHECK(edge_index[156]>=32); // latest pose now, same anchor
    // Returning from a road-qualified far pose cannot retain an old vertical
    // bias and lift the near rider above its authored grid/combat ground.
    word(0x7000,144);projected_actor(460,11u<<16,7447,93);
    word(0x7000,102);projected_actor(464,8u<<16,16384,138);
    wide_layer.line=134;CHECK(extent()==16);CHECK(std::abs(center()-155.5)<.6);
    // Enlarging nearby scenery must not pull the HUD viewport clip upwards.
    rr_geometry_reset();ram.fill(0);v=GVDP{};v.reg[5]=0x60;v.reg[12]=1;v.scanline=130;
    rom[0x3bf30]=1;rom[0x3bf31]=0x80;rom[0x180]=4;rom[0x181]=40;rom[0x182]=8;rom[0x183]=40;
    for(int y=0;y<40;++y)for(int x=0;x<4;++x)rom[0x186+y*5+x]=0x11;
    word(0x7000,(unsigned)(uint16_t)-352);word(0x7002,170);word(0x7004,16384);word(0x7006,152);word(0x7008,5);word(0x700a,0x80);
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    word(0x8000,258);word(0x8002,2<<8);word(0x8004,0xa001);word(0x8006,284);
    std::memset(ram.data()+0x9000,0x11,96);std::memset(v.vram+32,0x11,96);std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
    for(unsigned frame:{100u,104u}){rr_geometry_frame(frame,1);word(0xde64,0);if(frame==104)word(0x7004,20480);
        rr_geometry_scenery(0,0xff7000,0x1234,0,ram.data());rr_geometry_job_begin(0,0xff7000,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());CHECK(sprite(nullptr,&v,0,bounds));}
    rr_geometry_frame(105,1);v.scanline=151;CHECK(sprite(nullptr,&v,0,bounds));
    wide_layer.line=151;edge_op.fill(0);objects(nullptr,&v,&wide_layer);CHECK(edge_op[213]);
    std::puts("Organic scene: native assemblies, uncropped wide actors, paired scenery and terrain passed");
}
