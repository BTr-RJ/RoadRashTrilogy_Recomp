// 0.70 Motorcycle Motion prototype (RR1 rivals): pose-swap easing.
//   motorcycle_motion_test          regression checks
//   motorcycle_motion_test native   RR_GEOMETRY=0 keeps the original path
//   motorcycle_motion_test bench    synthetic worst-case processing cost
#include "sprite_geometry.h"
extern "C" {
#include "genesis_vdp.h"
}
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static GVDPVisualSprite sprite;
static GVDPHostSprites objects;
static GVDPVisualAudit audit;
extern "C" void gvdp_set_visual_sprites(GVDPVisualSprite fn,void*){sprite=fn;}
extern "C" void gvdp_set_visual_scroll(GVDPVisualScroll,void*){}
extern "C" void gvdp_set_visual_objects(GVDPHostSprites fn,void*){objects=fn;}
extern "C" void gvdp_set_visual_surface(GVDPVisualSurface,void*){}
extern "C" void gvdp_set_visual_audit(GVDPVisualAudit fn,void*){audit=fn;}
extern "C" void gvdp_set_visual_viewport(GVDPVisualViewport,void*){}
extern "C" const uint8_t *gvdp_sprite_patterns(unsigned,unsigned *bytes){*bytes=0;return nullptr;}
extern "C" int gvdp_sprite_pattern_line(unsigned,unsigned){return 0;}
extern "C" const uint32_t *gvdp_host_palette(void){return nullptr;}
extern "C" int gvdp_set_host_palette(const uint32_t*,unsigned){return 1;}

static std::array<uint8_t,65536> ram;
static GVDP v;
static std::vector<uint8_t> rom(0x3c000);
static void word(unsigned at,unsigned value){ram[at]=(uint8_t)(value>>8);ram[at+1]=(uint8_t)value;}
static void longword(unsigned at,unsigned value){word(at,value>>16);word(at+2,value);}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"motorcycle motion line %d: %s\n",__LINE__,#x);return 1;}}while(0)

constexpr int W=428,TOP=80,BOTTOM=152,ROWS=BOTTOM-TOP;
constexpr uint32_t BLANK=0xff123456;
enum Path {SOURCE,NATIVE}; // wide source-art drawing / 4:3 committed native bitmap
struct Shot {
    std::array<std::array<uint8_t,W>,ROWS> index{},opaque{};
    std::array<std::array<uint32_t,W>,ROWS> rgba{};
    bool operator==(const Shot& o) const {return index==o.index&&opaque==o.opaque&&rgba==o.rgba;}
    int count(int row) const {return (int)std::count(opaque[row].begin(),opaque[row].end(),(uint8_t)1);}
    double center(int row) const {double sum=0,n=0;for(int x=0;x<W;++x)if(opaque[row][x]){sum+=x;++n;}return n?sum/n:-1;}
    double rgba_center(int row) const {double sum=0,n=0;for(int x=0;x<W;++x)if(rgba[row][x]!=BLANK){sum+=x;++n;}return n?sum/n:-1;}
    bool only(uint8_t color) const {bool any=false;
        for(int r=0;r<ROWS;++r)for(int x=0;x<W;++x)if(opaque[r][x]){any=true;if(index[r][x]!=color)return false;}
        return any;}
};
struct Update {unsigned tick,image;uint8_t fill;};
struct Rider {uint32_t actor,job;int x;};

// A 40x40 source image, anchored at its bottom: an 8px column whose rows are
// displaced by `slope` pixels per pixel of height (a leaning rider).
static void make_art(unsigned image,unsigned at,double slope,unsigned nibble){
    unsigned entry=0x3bf1a+image*4;
    rom[entry]=(uint8_t)(at>>24);rom[entry+1]=(uint8_t)(at>>16);rom[entry+2]=(uint8_t)(at>>8);rom[entry+3]=(uint8_t)at;
    rom[at]=12;rom[at+1]=40;rom[at+2]=40;rom[at+3]=40;rom[at+4]=0x20;
    for(int y=0;y<40;++y){int d=(int)std::lround((39.5-y)*slope);
        for(int x=0;x<40;++x)if(x>=8+d&&x<16+d)rom[at+6+y*21+x/2]|=(uint8_t)(nibble<<((x&1)?0:4));}
}
static Shot capture(Path path,bool coverage){
    Shot shot;std::array<uint8_t,W> high{},owner{},bg{},bg_owner{},plane{};
    for(int line=TOP;line<BOTTOM;++line){int r=line-TOP;high.fill(0);owner.fill(0);
        GVDPSpriteLayer layer={line,path==SOURCE?W:320,path==SOURCE?54:0,shot.index[r].data(),shot.opaque[r].data(),high.data(),owner.data()};
        if(objects)objects(nullptr,&v,&layer);
        for(int x=0;x<W;++x)if(!shot.opaque[r][x])shot.index[r][x]=0;
        shot.rgba[r].fill(BLANK);
        if(coverage&&audit){GVDPVisualAuditRow row{};row.line=line;row.total=layer.total;row.offset=layer.offset;
            row.background_index=bg.data();row.background_owner=bg_owner.data();row.plane_high=plane.data();
            audit(nullptr,&v,&row);rr_geometry_resolve(&v,line,shot.rgba[r].data(),layer.total);}
    }
    return shot;
}
// Replays native updates at their ticks and renders every display frame.
static std::vector<Shot> run(const char *mode,Path path,const std::vector<Update>& updates,unsigned last,
                             unsigned scale=16384,bool coverage=false,const std::vector<Rider>& riders={{0xff2300,0xff7000,160}},
                             bool keep=true){
    _putenv_s("RR_MOTORCYCLE_MOTION",mode?mode:"");_putenv_s("RR_WIDE",path==SOURCE?"1":"0");
    rr_geometry_enable_coverage(coverage?1:0);
    rr_geometry_reset();ram.fill(0);v=GVDP{};v.reg[5]=0x60;v.reg[12]=1;v.reg[1]=0x40;
    v.cram[17]=0x000e;v.cram[18]=0x00e0;
    longword(0x1e68,10u<<16);                                    // camera Z
    for(unsigned i=0;i<32;++i)word(0xd77c+i*2,82+128/(2+i));     // flat road profile
    longword(0x38ac,0xff8000);longword(0x38d0,0xff9000);word(0xd34a,1);
    for(const auto& rider:riders){unsigned a=rider.actor&65535,j=rider.job&65535;
        longword(a+4,(10u<<16)+32768);ram[a+104]=0x40;           // depth 2.5, alive
        word(j,(unsigned)rider.x);word(j+2,133);word(j+4,scale);word(j+6,224);word(j+10,0x80);}
    std::vector<Shot> shots;size_t next=0;
    for(unsigned tick=updates.front().tick;tick<=last;++tick){
        rr_geometry_frame(tick,1);
        if(next<updates.size()&&updates[next].tick==tick){const auto& u=updates[next++];
            for(const auto& rider:riders){word((rider.job&65535)+8,u.image);
                if(path==NATIVE){
                    // One committed 32x32 native piece standing on the anchor.
                    std::memset(ram.data()+0x9000,u.fill,512);std::memcpy(v.vram+32,ram.data()+0x9000,512);
                    word(0x8000,229);word(0x8002,0x0f00);word(0x8004,0xa001);word(0x8006,272);
                    std::memcpy(v.vram+0xc000,ram.data()+0x8000,8);
                    rr_geometry_actor_begin(0,rider.job,rider.actor);rr_geometry_actor(0,rider.job,rider.actor,ram.data());
                    rr_geometry_actor_end(0,rider.job+20,ram.data());
                    word(0xde64,0);rr_geometry_job_begin(0,rider.job,ram.data());word(0xde64,1);rr_geometry_job_end(ram.data());
                }else{
                    rr_geometry_actor_begin(0,rider.job,rider.actor);rr_geometry_actor(0,rider.job,rider.actor,ram.data());
                    rr_geometry_actor_end(0,rider.job,ram.data());
                }
            }
        }
        v.scanline=100;double bounds[4];if(sprite)sprite(nullptr,&v,0,bounds);
        Shot shot=capture(path,coverage);
        if(keep||tick==last)shots.push_back(shot);
    }
    return shots;
}
static double mean_distance(const Shot& a,const Shot& b){double sum=0,n=0;
    for(int r=0;r<ROWS;++r)if(a.count(r)&&b.count(r)){sum+=std::abs(a.center(r)-b.center(r));++n;}
    return n?sum/n:0;}

int main(int argc,char **argv){
    std::string command=argc>1?argv[1]:"";
    if(command=="native"){
        // Geometry off: the whole presentation layer stays out, option or not.
        _putenv_s("RR_GEOMETRY","0");_putenv_s("RR_MOTORCYCLE_MOTION","1");
        rr_geometry_source(0,rom.data(),(unsigned)rom.size(),ram.data());rr_geometry_frame(0,1);
        CHECK(!sprite&&!objects&&!audit);
        std::puts("motorcycle motion: geometry off keeps the native path");return 0;
    }
    _putenv_s("RR_GEOMETRY","1");_putenv_s("RR_ORGANIC","1");_putenv_s("RR_ARCADE_MOTION","1");
    constexpr unsigned UPRIGHT=4,LEAN=5,FALL=6;
    make_art(UPRIGHT,0x1000,0.,1);make_art(LEAN,0x1400,.2,2);make_art(FALL,0x1800,.5,3);
    rr_geometry_source(0,rom.data(),(unsigned)rom.size(),ram.data());
    // Native updates every five frames; the pose image changes at tick 420.
    constexpr unsigned SWAP=420,FIRST=400,LAST=432;
    std::vector<Update> lean;for(unsigned t=FIRST;t<=430;t+=5)lean.push_back({t,t<SWAP?UPRIGHT:LEAN,(uint8_t)(t<SWAP?0x11:0x22)});
    auto at=[&](const std::vector<Shot>& shots,unsigned tick)->const Shot&{return shots[tick-FIRST];};

    if(command=="bench"){
        // Worst case: six riders, a pose change on EVERY native update, so a
        // residual is active on every frame. Includes the coverage resolver.
        std::vector<Rider> riders;for(int n=0;n<6;++n)riders.push_back({0xff2300u+0x80u*n,0xff7000u+20u*n,60+n*40});
        constexpr unsigned FRAMES=1500;
        std::vector<Update> busy;for(unsigned t=FIRST;t<FIRST+FRAMES;t+=5)busy.push_back({t,((t/5)&1)?LEAN:UPRIGHT,0x11});
        double best[4]={1e30,1e30,1e30,1e30};const char *modes[4]={"0","1","2","3"};
        for(int pass=0;pass<5;++pass)for(int mode=0;mode<4;++mode){
            auto begin=std::chrono::steady_clock::now();
            run(modes[mode],SOURCE,busy,FIRST+FRAMES-1,16384,true,riders,false);
            double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/FRAMES;
            best[mode]=std::min(best[mode],us);
        }
        std::printf("bench frames=%u riders=6 classic_us_per_frame=%.2f enhanced_us_per_frame=%.2f delta_us=%.2f delta_percent=%.2f frame_budget_percent=%.4f\n",
            FRAMES,best[0],best[1],best[1]-best[0],(best[1]-best[0])*100/best[0],(best[1]-best[0])*100/16683.);
        std::printf("bench prototype02_us_per_frame=%.2f delta_us=%.2f delta_percent=%.2f frame_budget_percent=%.4f\n",
            best[2],best[2]-best[0],(best[2]-best[0])*100/best[0],(best[2]-best[0])*100/16683.);
        std::printf("bench prototype03_us_per_frame=%.2f delta_us=%.2f delta_percent=%.2f frame_budget_percent=%.4f\n",
            best[3],best[3]-best[0],(best[3]-best[0])*100/best[0],(best[3]-best[0])*100/16683.);
        return 0;
    }

    for(Path path:{SOURCE,NATIVE}){
        auto unset=run(nullptr,path,lean,LAST),classic=run("0",path,lean,LAST),enhanced=run("1",path,lean,LAST);
        // 1. Classic equivalence: absent and 0 are the same picture on every
        // frame, and the enhanced mode is identical outside the residual.
        CHECK(unset==classic);
        CHECK(at(classic,SWAP-1).count(40)>0);                  // the fixture really draws a rider
        for(unsigned t=FIRST;t<SWAP;++t)CHECK(at(enhanced,t)==at(classic,t));
        // 2. Swap synchronisation: the new image is on screen at the SAME
        // native tick in both modes; nothing of the old image survives.
        uint8_t before=17,after=18;
        CHECK(at(classic,SWAP-1).only(before)&&at(enhanced,SWAP-1).only(before));
        CHECK(at(classic,SWAP).only(after)&&at(enhanced,SWAP).only(after));
        // 3. Silhouette, transparency and limits: same rows, same opaque run
        // length per row, same hill/viewport clip. Only whole rows move.
        for(unsigned t=SWAP;t<=LAST;++t)for(int r=0;r<ROWS;++r)CHECK(at(enhanced,t).count(r)==at(classic,t).count(r));
        // 4. Convergence: the residual shrinks every frame and is exactly zero
        // four frames after the swap (five-frame native interval).
        CHECK(!(at(enhanced,SWAP)==at(classic,SWAP)));
        double previous=1e9;
        for(unsigned t=SWAP;t<SWAP+4;++t){double d=mean_distance(at(enhanced,t),at(classic,t));CHECK(d>0&&d<previous);previous=d;}
        for(unsigned t=SWAP+4;t<=LAST;++t)CHECK(at(enhanced,t)==at(classic,t));
        // 5. Continuity: classic jumps the whole lean in one frame; enhanced
        // never moves a row by more than a share of it. Alignment at the
        // ground anchor (bottom row) is untouched.
        double classic_jump=0,enhanced_jump=0;
        for(unsigned t=SWAP;t<=SWAP+4;++t)for(int r=0;r<ROWS;++r)if(at(classic,t).count(r)&&at(classic,t-1).count(r)){
            classic_jump=std::max(classic_jump,std::abs(at(classic,t).center(r)-at(classic,t-1).center(r)));
            enhanced_jump=std::max(enhanced_jump,std::abs(at(enhanced,t).center(r)-at(enhanced,t-1).center(r)));}
        if(path==SOURCE){
            CHECK(classic_jump>=7.5);CHECK(enhanced_jump<=2.5);
            // Each row starts 80% of the way back to the previous pose.
            for(int r=0;r<ROWS;++r)if(at(classic,SWAP).count(r)){
                double lean_px=at(classic,SWAP).center(r)-at(classic,SWAP-1).center(r);
                double moved=at(enhanced,SWAP).center(r)-at(classic,SWAP).center(r);
                CHECK(std::abs(moved+.8*lean_px)<=1.01);}
        }else{
            // The committed native bitmap is displaced by the same pose line:
            // -0.8*0.2 px per pixel of height, none at the ground row. (This
            // synthetic block does not lean itself, so only the residual of
            // the ROM pose line is visible here.)
            CHECK(classic_jump==0&&enhanced_jump>0);
            int top=0;while(top<ROWS&&!at(classic,SWAP).count(top))++top;
            int bottom=ROWS-1;while(bottom>0&&!at(classic,SWAP).count(bottom))--bottom;
            CHECK(bottom-top==31);
            double high=at(enhanced,SWAP).center(top)-at(classic,SWAP).center(top);
            double low=at(enhanced,SWAP).center(bottom)-at(classic,SWAP).center(bottom);
            CHECK(std::abs(high+.8*.2*31.5)<=1.01);CHECK(std::abs(low)<=.51);
        }
    }
    // 6. Chained lean steps (a second change before the first residual ends)
    // continue from the displayed pose: no jump back, no overshoot.
    {std::vector<Update> chain=lean;chain.resize(4);
        chain.push_back({420,LEAN,0x22});chain.push_back({423,UPRIGHT,0x11});chain.push_back({428,UPRIGHT,0x11});
        auto classic=run("0",SOURCE,chain,LAST),enhanced=run("1",SOURCE,chain,LAST);
        int top=0;while(top<ROWS&&!at(classic,419).count(top))++top;
        double rest=at(classic,419).center(top),last=rest;
        CHECK(at(classic,423).only(17)&&at(enhanced,423).only(17));
        for(unsigned t=420;t<=422;++t){double c=at(enhanced,t).center(top);CHECK(c>last&&c-last<=2.5);last=c;}
        for(unsigned t=423;t<=425;++t){double c=at(enhanced,t).center(top);CHECK(c<last&&last-c<=2.5);last=c;}
        CHECK(last==rest);
        for(unsigned t=425;t<=LAST;++t)CHECK(at(enhanced,t)==at(classic,t));
    }
    // 7. Safe classic fallbacks, with the option ON.
    {   // A change too large to be a lean step keeps the committed presentation.
        std::vector<Update> fall;for(unsigned t=FIRST;t<=430;t+=5)fall.push_back({t,t<SWAP?UPRIGHT:FALL,0x11});
        CHECK(run("1",SOURCE,fall,LAST)==run("0",SOURCE,fall,LAST));
        // Focal 3 is a car, not a motorcycle.
        CHECK(run("1",SOURCE,lean,LAST,19661)==run("0",SOURCE,lean,LAST,19661));
        // Any value other than 1, 2, 3 (prototypes) or 4 (enhanced) is classic.
        CHECK(run("5",SOURCE,lean,LAST)==run("0",SOURCE,lean,LAST));
        CHECK(run("12",SOURCE,lean,LAST)==run("0",SOURCE,lean,LAST));
    }
    // 9. Prototype 02 (=2): reconstructed in-between frames, ten frames long.
    auto run_length=[](const Shot& s,int r){int first=-1,last=-1;
        for(int x=0;x<W;++x)if(s.opaque[r][x]){if(first<0)first=x;last=x;}
        return first<0?0:last-first+1;};
    // A committed native bitmap (4:3) keeps the native swap: in-betweens are
    // only built from the source image classic already draws.
    CHECK(run("2",NATIVE,lean,LAST)==run("0",NATIVE,lean,LAST));
    for(Path path:{SOURCE}){
        auto classic=run("0",path,lean,LAST),morph=run("2",path,lean,LAST);
        // Classic outside the transition, exact native pose at its end.
        for(unsigned t=FIRST;t<SWAP;++t)CHECK(at(morph,t)==at(classic,t));
        for(unsigned t=SWAP+9;t<=LAST;++t)CHECK(at(morph,t)==at(classic,t));
        double previous=-1;
        for(unsigned t=SWAP;t<SWAP+9;++t){const Shot& s=at(morph,t);
            // No ghost: ONE source image per frame, switched at the midpoint
            // (frames 0-3 the previous image, 4-8 the new one).
            CHECK(s.only(t<SWAP+4?17:18));
            // Crisp pixel art: every row is one solid run of the original
            // 8-pixel width (+-1 from nearest resampling), no holes.
            int top=-1;
            for(int r=0;r<ROWS;++r)if(s.count(r)){if(top<0)top=r;
                CHECK(s.count(r)==run_length(s,r));CHECK(s.count(r)>=7&&s.count(r)<=9);}
            CHECK(top>=0);
            // The silhouette travels monotonically, never more than 2.5px a
            // frame at its fastest row (classic: 8px in one frame).
            double center=s.center(top);
            if(previous>=0)CHECK(center>=previous&&center-previous<=2.5);
            previous=center;
        }
        if(path==SOURCE){
            int top=0;while(top<ROWS&&!at(classic,SWAP).count(top))++top;
            double from=at(classic,SWAP-1).center(top),to=at(classic,SWAP).center(top);
            CHECK(to-from>=7.5);
            CHECK(at(morph,SWAP).center(top)-from<=1.01);              // starts at the old pose
            double middle=at(morph,SWAP+4).center(top);
            CHECK(middle>from+2&&middle<to-2);                         // a real in-between
            CHECK(to-at(morph,SWAP+8).center(top)<=1.01);              // arrives at the new pose
        }
    }
    {   // A second change during the morph continues from the displayed shape.
        std::vector<Update> chain=lean;chain.resize(4);
        for(unsigned t:{420u,423u,428u,433u})chain.push_back({t,t==420?LEAN:UPRIGHT,(uint8_t)(t==420?0x22:0x11)});
        constexpr unsigned END=436;
        auto classic=run("0",SOURCE,chain,END),morph=run("2",SOURCE,chain,END);
        int top=0;while(top<ROWS&&!at(classic,419).count(top))++top;
        double rest=at(classic,419).center(top),last=rest,peak=rest;
        for(unsigned t=420;t<=END;++t){double c=at(morph,t).center(top);
            CHECK(std::abs(c-last)<=2.5);CHECK(at(morph,t).only(17));last=c;peak=std::max(peak,c);}
        CHECK(peak>rest&&peak<rest+4);CHECK(last==rest);
        for(unsigned t=432;t<=END;++t)CHECK(at(morph,t)==at(classic,t));
    }
    {   // Fallbacks with prototype 02 ON: too different an image, and a car.
        std::vector<Update> fall;for(unsigned t=FIRST;t<=430;t+=5)fall.push_back({t,t<SWAP?UPRIGHT:FALL,0x11});
        make_art(7,0x1c00,.75,3);
        std::vector<Update> wreck;for(unsigned t=FIRST;t<=430;t+=5)wreck.push_back({t,t<SWAP?UPRIGHT:7u,0x11});
        CHECK(!(run("2",SOURCE,fall,LAST)==run("0",SOURCE,fall,LAST)));   // 20px is still a lean step here
        CHECK(run("2",SOURCE,wreck,LAST)==run("0",SOURCE,wreck,LAST));     // 30px is not
        CHECK(run("2",SOURCE,lean,LAST,19661)==run("0",SOURCE,lean,LAST,19661));
    }
    {   // Release raster: the in-between is a real intermediate picture, with
        // no colour that neither pose has (nothing blended between images).
        auto classic=run("0",SOURCE,lean,LAST,16384,true),morph=run("2",SOURCE,lean,LAST,16384,true);
        int top=0;while(top<ROWS&&at(classic,SWAP).rgba_center(top)<0)++top;
        double from=at(classic,SWAP-1).rgba_center(top),to=at(classic,SWAP).rgba_center(top),middle=at(morph,SWAP+4).rgba_center(top);
        CHECK(middle>from+2&&middle<to-2);
        std::vector<uint32_t> allowed;
        for(unsigned t:{SWAP-1,SWAP})for(const auto& row:at(classic,t).rgba)for(uint32_t c:row)
            if(std::find(allowed.begin(),allowed.end(),c)==allowed.end())allowed.push_back(c);
        for(unsigned t=SWAP;t<SWAP+9;++t)for(const auto& row:at(morph,t).rgba)for(uint32_t c:row)
            CHECK(std::find(allowed.begin(),allowed.end(),c)!=allowed.end());
        for(unsigned t=SWAP+9;t<=LAST;++t)CHECK(at(morph,t)==at(classic,t));
    }
    // 8. The release raster (area coverage) follows the same residual.
    for(Path path:{SOURCE,NATIVE}){
        auto classic=run("0",path,lean,LAST,16384,true),enhanced=run("1",path,lean,LAST,16384,true);
        int top=0;while(top<ROWS&&at(classic,SWAP).rgba_center(top)<0)++top;
        CHECK(top<ROWS);
        for(unsigned t=FIRST;t<SWAP;++t)CHECK(at(enhanced,t)==at(classic,t));
        double moved=at(enhanced,SWAP).rgba_center(top)-at(classic,SWAP).rgba_center(top);
        CHECK(moved<-3.5&&moved>-7.5);
        for(unsigned t=SWAP+4;t<=LAST;++t)CHECK(at(enhanced,t)==at(classic,t));
    }
    // 10. Prototype 03 (=3): velocity-continuous follower (rivals here; the player
    // uses the same follower on its native bitmap and is qualified on the real game).
    {
        constexpr unsigned END=470;
        CHECK(run("3",NATIVE,lean,LAST)==run("0",NATIVE,lean,LAST));          // 4:3 bitmap keeps the native swap
        auto classic=run("0",SOURCE,lean,END),follow=run("3",SOURCE,lean,END);
        for(unsigned t=FIRST;t<SWAP;++t)CHECK(at(follow,t)==at(classic,t));
        int top=0;while(top<ROWS&&!at(classic,SWAP).count(top))++top;
        CHECK(top<ROWS);
        double from=at(classic,SWAP-1).center(top),to=at(classic,SWAP).center(top);
        CHECK(to-from>=7.5);
        double previous=from,velocity=0,peak_accel=0,peak_step=0;unsigned arrived=0;
        for(unsigned t=SWAP;t<=END;++t){const Shot& s=at(follow,t);
            if(s==at(classic,t)){if(!arrived)arrived=t;continue;}
            CHECK(!arrived);                                   // once native again, it stays native
            CHECK(s.only(17)||s.only(18));                     // ONE source image: no ghost, no blend
            int first=-1;
            for(int r=0;r<ROWS;++r)if(s.count(r)){if(first<0)first=r;CHECK(s.count(r)==run_length(s,r));CHECK(s.count(r)>=7&&s.count(r)<=9);}
            CHECK(first>=0);
            double c=s.center(top),step=c-previous;
            CHECK(step>=-.01);                                 // monotonic travel, no overshoot
            peak_step=std::max(peak_step,step);
            if(t>SWAP)peak_accel=std::max(peak_accel,std::abs(step-velocity));
            velocity=step;previous=c;
        }
        CHECK(arrived&&arrived<=SWAP+30);                      // converges exactly to the native pose
        CHECK(at(follow,SWAP).center(top)-from<=1.01);         // starts at the old pose
        CHECK(peak_step<=2.5&&peak_accel<=1.2);                // classic: 8px in ONE frame
        double middle=at(follow,SWAP+4).center(top);
        CHECK(middle>from+1&&middle<to-1);
        // A second native change mid-transition keeps the velocity (no restart from rest).
        std::vector<Update> chain=lean;chain.resize(4);
        for(unsigned t:{420u,425u,430u,435u})chain.push_back({t,(t==420||t==430)?LEAN:UPRIGHT,(uint8_t)((t==420||t==430)?0x22:0x11)});
        auto morph=run("3",SOURCE,chain,END),base=run("0",SOURCE,chain,END);
        double last=at(base,419).center(top),slope=0;
        for(unsigned t=420;t<=END&&!(at(morph,t)==at(base,t));++t){double c=at(morph,t).center(top);
            CHECK(std::abs(c-last)<=3.5);CHECK(std::abs((c-last)-slope)<=1.6);slope=c-last;last=c;}
    }
    // 11. Enhanced (=4): rivals are exactly prototype 02; native fallback elsewhere.
    {
        CHECK(run("4",NATIVE,lean,LAST)==run("0",NATIVE,lean,LAST));
        CHECK(run("4",SOURCE,lean,LAST,19661)==run("0",SOURCE,lean,LAST,19661));
        for(unsigned end:{LAST,470u}){CHECK(run("4",SOURCE,lean,end)==run("2",SOURCE,lean,end));}
        std::vector<Update> fall;for(unsigned t=FIRST;t<=430;t+=5)fall.push_back({t,t<SWAP?UPRIGHT:FALL,0x11});
        CHECK(run("4",SOURCE,fall,LAST)==run("2",SOURCE,fall,LAST));
    }
    rr_geometry_enable_coverage(0);
    std::puts("motorcycle motion: classic equivalence, swap sync, convergence and fallbacks OK");
    return 0;
}
