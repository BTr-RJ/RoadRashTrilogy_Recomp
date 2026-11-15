#include "sprite_geometry.h"
#include "sprite_assets.h"
#include "road_surface.h"
#include "road_markings.h"
#include "race_view.h"
#include "continuous_value.h"
extern "C" {
#include "genesis_vdp.h"
#include "genesis_dac.h"
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>

namespace {
bool coverage_enabled=false;
// Every camera owns a complete presentation history, including actor artwork.
struct GeometryView {
    std::vector<rr::CoverageSprite> coverage_sprites;
    std::array<uint32_t,GVDP_MAX_WIDTH> coverage_colors{};
    std::array<uint8_t,GVDP_MAX_WIDTH> coverage_mask{};
    int coverage_line=-1,coverage_width=0;
    uint64_t coverage_frame=UINT64_MAX;
    int view_id=0;
    int view_left=0,view_right=320,view_top=0,view_bottom=152;
    double view_center=160,view_horizon=64;
    double view_focal=1;
    rr::RaceView view;
    int left_edge() const {return view_left-(view_id==2?0:margin());}
    int right_edge() const {return view_right+(view_id==1?0:margin());}
    int view_margin=0;
    int margin() const {return view_margin;}
    bool row_inside(int y) const {return y>=view_top&&y<view_bottom;}
    void configure(int game){
        view=rr::race_view(game,view_id);view_left=view.left;view_right=view.right;
        view_top=view.top;view_bottom=view.bottom;view_center=view.center;view_horizon=view.horizon;view_focal=view.focal;
    }
    ~GeometryView(){if(initialized){finish();close_packet_trace();}}
    std::map<std::string,std::string> trace_paths;
    const char *trace_path(const char *name){
        const char *value=std::getenv(name);if(!value||!*value)return nullptr;
        if(!view_id)return value;
        auto& path=trace_paths[name];path=std::string(value)+".p"+std::to_string(view_id);return path.c_str();
    }
struct Profile {unsigned count,sat,patterns,tile_base;};
inline static constexpr Profile profiles[]={{0xde64,0x38ac,0x38d0,0xd34a},
    {0xdc34,0x27a6,0x27ce,0xce86},{0x4fc2,0x277e,0x27a6,0x36ea}};
unsigned word(const uint8_t *r,unsigned a){return (r[a&65535]<<8)|r[(a+1)&65535];}
unsigned longword(const uint8_t *r,unsigned a){return (word(r,a)<<16)|word(r,a+2);}
uint32_t hash_byte(uint32_t hash,unsigned byte){return (hash^byte)*16777619u;}
struct Piece {std::array<uint8_t,8> sat{};uint32_t pixels=0;};
struct Projection {
    uint64_t key=0,tick=0;
    double z=0,camera=0;bool scenery=false;
    unsigned image=0,second=0,flags=0,relative_z=0;
    uint32_t actor=0;unsigned component=0,components=0;uint64_t epoch=0;bool player=false;
    rr::RoadRays road;bool has_road=false;
    double size_offset=0;
};
struct ActorPlan {uint64_t tick=0,epoch=0;unsigned components=0;bool player=false;};
struct SourcePart {double x=0,y=0,scale=1,clip=152;unsigned image=0,second=0,flags=0;uint32_t address=0;};
struct ActorScope {int game=-1;uint32_t actor=0,job=0;std::vector<SourcePart> sources;};
ActorScope actor_scope;
struct SourcePacket {uint64_t tick=0,epoch=0;std::vector<SourcePart> parts;double world=0,camera=0,lateral=0,height=0;bool alive=false;
    rr::RoadRays road;bool has_road=false;unsigned render_state=0;};
std::map<uint32_t,std::deque<SourcePacket>> actor_sources;
std::map<uint32_t,SourcePacket> early_sources;
std::map<uint32_t,ActorPlan> actor_plans;
uint64_t actor_epoch=0;
FILE *packet_trace=nullptr;bool packet_trace_initialized=false;
void close_packet_trace(){if(packet_trace){std::fclose(packet_trace);packet_trace=nullptr;}}
void trace_packet(uint32_t actor,const SourcePacket& packet,unsigned count,unsigned scoped,unsigned early){
    if(!packet_trace_initialized){packet_trace_initialized=true;
        if(const char *path=trace_path("RR_ACTOR_PACKETS"))if((packet_trace=std::fopen(path,"wb"))){
            std::fputs("frame,actor,epoch,components,scoped,early,part,address,image,second,x,y,scale,clip,world,camera\n",packet_trace);
            
        }}
    if(!packet_trace)return;
    for(unsigned i=0;i<packet.parts.size();++i){const auto& p=packet.parts[i];
        std::fprintf(packet_trace,"%llu,%06X,%llu,%u,%u,%u,%u,%06X,%u,%u,%.4f,%.4f,%.7f,%.4f,%.7f,%.7f\n",
            (unsigned long long)packet.tick,actor,(unsigned long long)packet.epoch,count,scoped,early,i,p.address,
            p.image,p.second,p.x,p.y,p.scale,p.clip,packet.world,packet.camera);
    }
}
struct Job {
    uint64_t serial=0,tick=0,key=0;uint32_t base=0,address=0;
    int first=0;double x=0,y=0,scale=1,depth=0,clip=152;unsigned image=0,second=0,flags=0;
    Projection projection;
    std::vector<Piece> pieces;
};
struct Pose {double x=0,y=0,scale=1;};
struct History {
    Pose previous,current;uint64_t serial=0,changed=0,seen=0;
    unsigned image=0,flags=0;double duration=1;bool interpolate=false;
    std::deque<std::pair<uint64_t,Pose>> samples;
    Job art;
    rr::QuantizedAnchor anchor_x,anchor_y;
};
struct Bounds {double x0=0,y0=0,x1=0,y1=0;bool valid=false;};
struct Scroll {
    std::array<int,4> previous{},current{};
    uint64_t epoch=0;bool valid=false;
    std::deque<std::pair<uint64_t,std::array<int,4>>> samples;
};
struct RoadEvent {uint64_t tick;rr::RoadRays rays;};
std::deque<RoadEvent> roads;
std::deque<std::pair<uint64_t,double>> cameras;
std::deque<std::pair<uint64_t,double>> vehicle_cameras;
uint64_t vehicle_camera_frame=UINT64_MAX;double vehicle_camera_value=0,vehicle_camera_native=0;
rr::ContinuousValue camera_filter;
struct SurfaceRow {uint64_t tick=UINT64_MAX;double u=0,du=0,v=0,footprint=0;bool valid=false;};
std::array<SurfaceRow,240> surface_rows;
std::array<double,240> road_centers{};
std::array<uint8_t,64> terrain_texture{};
std::array<bool,64> terrain_colors{};
bool terrain_ready=false;
struct LaneRow {uint64_t tick=UINT64_MAX;double center=0,z=0,depth=1,camera=0,width=0;int asphalt=57,color=60;bool valid=false,painted=false;};
std::array<LaneRow,240> lanes;
struct LaneStroke {int left=9999,right=-9999,color=60;};
std::array<LaneStroke,240> native_strokes;
std::array<bool,80> marking_slots{};
bool markings_ready=false;
rr::MarkingModel lane_model;
double lane_period_hint=0;
uint64_t lane_fit_epoch=UINT64_MAX,lane_pixels=0;
uint64_t terrain_pixels=0;
std::array<Scroll,240> scrolls;
uint64_t plane_frame=UINT64_MAX,plane_changed=0,plane_epoch=0;
uint32_t plane_hash=0;
double plane_duration=1;
std::deque<Job> pending;
std::map<uint64_t,History> histories;
std::map<uint32_t,Projection> projections;
int game_index=-1;
const uint8_t *source_rom=nullptr,*source_ram=nullptr;
unsigned source_bytes=0;
bool organic=false;
inline static constexpr unsigned asset_tables[]={0x3bf1a,0x56a0a,0x9c7c8};
std::map<unsigned,rr::SpriteAsset> assets;
struct Billboard {const rr::SpriteAsset *art;Pose pose;int flags,palette,priority;double z,clip;unsigned order;};
std::vector<Billboard> billboards;
struct ActorCanvas {int x=0,y=0,width=0,height=0;std::vector<uint8_t> index,opaque,high;rr::CoverageImage coverage;};
struct ActorArt {const rr::SpriteAsset *art;SourcePart source;unsigned attr,base;bool native;};
// 0.70 Motorcycle Motion (RR1 rivals). Center line of a rider image in source
// pixels: x(h)=offset+slope*h, h measured upward from the ground anchor row.
struct PoseLine {double offset=0,slope=0,height=0;bool valid=false;};
// Presentation-only residual between the previous and the current pose image.
// It never selects artwork and never changes when a native image is committed.
struct PoseEase {unsigned image=0,flags=0;bool known=false,active=false;PoseLine line;
    uint64_t committed=0,start=0;double offset=0,slope=0,duration=1;};
// Prototype 02 (RR_MOTORCYCLE_MOTION=2): reconstructed in-between frames.
// A pose silhouette is its left/right hull per normalised height, in source
// pixels relative to the ground anchor. In-betweens interpolate the hull and
// re-sample ONE source image with nearest texels: no blending, no ghost.
inline static constexpr int POSE_ROWS=64;
struct PoseShape {std::array<float,POSE_ROWS> left{},right{};double height=0;bool valid=false;};
struct PoseTexture {const rr::SpriteAsset *art=nullptr;bool flip=false;unsigned image=0,flags=0,attr=0,base=0;PoseShape shape;};
struct PoseMorph {PoseTexture from,to;PoseShape start;uint64_t committed=0,begin=0,built=UINT64_MAX;double duration=1;
    bool known=false,active=false;rr::SpriteAsset frame;ActorArt part{};
    // Prototype 03 (RR_MOTORCYCLE_MOTION=3): `shown` is the hull on screen. It
    // chases `goal` (the newest native hull) with a critically damped follower,
    // so velocity stays continuous across 12 Hz native swaps. `from` is the
    // texture on screen before the newest swap, `to` the newest native one.
    PoseShape shown,goal;PoseTexture shown_tex;std::array<float,POSE_ROWS> lv{},rv{};
    double hv=0,omega=.6;uint64_t stepped=UINT64_MAX;};
// Prototype 04 (RR_MOTORCYCLE_MOTION=6): temporal pixel dissolve. The two ROM
// images (old and newly committed) are kept in screen orientation, relative to
// the same ground anchor. For at most 3 frames each pixel switches ONCE from
// the old to the new image, in the order of a 4x4 Bayer matrix indexed by the
// sprite-local coordinates. No new colour, no resampling, no deformation.
struct DissolveImage {int width=0,height=0,anchor_x=0,anchor_y=0;std::vector<uint8_t> pixels;};
struct Dissolve {DissolveImage from,to,shown;unsigned id=0,second=0,flags=0,attr=0,base=0,from_id=0,frames=3;
    bool known=false,active=false;uint64_t committed=0,begin=0,built=UINT64_MAX;
    rr::SpriteAsset frame;ActorArt part{};};
struct ActorCache {ActorCanvas canvas;Pose root;uint64_t root_key=0,epoch=0,seen=0;unsigned first=0;
    uint32_t actor=0;
    PoseEase ease;PoseMorph morph;Dissolve dissolve;
    rr::ContinuousValue depth_filter;
    rr::QuantizedAnchor body_filter,height_filter;
    bool full_source=false,has_motion=false,world_height_valid=false,lane_anchor=false,raw_height=false,departing=false;
    double focal=0,height_bias=0,curve_center=160,lane_world=0,body_offset=0,departure_depth=0,departure_top=0;
    double departure_speed=2;uint64_t departure_frame=UINT64_MAX;
    double lane_target=0,lane_position=0,lane_velocity=0;uint64_t lane_frame=UINT64_MAX;
    History motion;SourcePacket appearance;std::map<uint64_t,unsigned> palettes;std::vector<ActorArt> parts;};
struct ActorDrawing {const ActorCache *cache;Pose root;double ratio;unsigned order;bool held;double clip=152;uint32_t actor=0;
    // Destination-row displacement in screen pixels; both zero in classic mode.
    double row_offset=0,row_slope=0;
    double row_shift(int line) const {return row_offset==0&&row_slope==0?0.:row_offset+row_slope*(root.y-(line+.5));}
    // Prototype 02: one reconstructed in-between image replacing the parts.
    const ActorArt *morph=nullptr;};
bool pose_morphing=false;double pose_morph_frames=10;
// Prototype 03: critically damped hull follower for rivals and the player.
bool pose_following=false,pose_player=false,pose_player_force=false;
double pose_follow_rival=.45,pose_follow_player=.7;   // 1/frame; rivals favour smoothness, the player (input-coupled) favours response
uint64_t player_serial=0;FILE *player_trace=nullptr,*player_jobs_trace=nullptr,*hull_trace=nullptr;
bool pose_dissolving=false;uint64_t dissolve_swaps=0,dissolve_frames=0;FILE *dissolve_trace=nullptr;   // prototype 04
// 0.70 D033 Rival Identity (RR1 first). Per-sprite palette composition only: the
// rider pixels keep their nibble, transparency, priority, order and geometry; the
// colour comes from the engine's host palette (never from the guest CRAM).
bool rival_identity=false;
std::vector<std::pair<uint32_t,int>> identity_slots;   // actor address -> host palette block (16 colours)
uint64_t identity_slots_tick=UINT64_MAX;
struct RivalLook {const char *name;uint32_t helmet,vest,suit;};   // 0xRRGGBB: main shade of helmet / vest / rider suit
// D034: owner-chosen kits (29 riders). Police (O'Leary, O'Rourke, O'Shea, O'Connor, Flynn) are NOT listed: they keep the original look.
inline static constexpr RivalLook rr1_looks[]={
    {"Lester",0xf08214,0x283c8c,0xd7d7dc},{"Rude Boy",0xf0289c,0x282828,0x8cc81e},
    {"Grubb",0x965a28,0xc8aa6e,0x5a6428},{"Gunther",0x28282d,0xdcb428,0x8c1e1e},
    {"Sid",0xe6b428,0x6e28a0,0x3c1e5a},{"Axle",0xdc2828,0x323237,0x8c9196},
    {"Hammer",0x28282d,0xf0d21e,0x5a5a64},{"Dread",0x28282d,0xaa1e28,0x3c3c44},
    {"Viper",0x28282d,0x8cf03c,0x3c5a28},{"Shiva",0x1ec8c8,0xdcaa28,0x28285a},
    {"Natasha",0xaae6f0,0xc8288c,0x28285a},{"Slater",0x6e82a0,0x1e2d5a,0xdcdce1},
    {"Biff",0xe65a1e,0x784628,0xf0f0f0},{"Rex",0xf0f0f0,0x784628,0xb4781e},
    {"Hoghide",0x786428,0x282828,0x5a3c1e},{"Ikira",0xaaaab4,0xaa1e50,0x1e1e28},
    {"Butch",0x5a6e32,0xc8be8c,0x3c4628},{"Angel",0x78bef0,0xf0c8dc,0xf0f0f0},
    {"BroomHelga",0x3cb43c,0x28282d,0x6e286e},{"Chip",0x1ec8e6,0xf08c1e,0x28325a},
    {"Sergio",0x8c1e3c,0xd2aa3c,0x28282d},{"Spike",0x6edc28,0x28282d,0x28465a},
    {"Zippy",0xdcf03c,0x286edc,0x28282d},{"Otis",0x8c5a28,0xebe6cd,0x46321e},
    {"Helldog",0x28282d,0xf08214,0x8c1e14},{"Bob",0x5a96e6,0x8c8c91,0xf0f0f0},
    {"Luna",0xbeaaf0,0x462878,0xe1e1f0},{"Killjoy",0x82f03c,0x64288c,0x28282d},
    {"Ollie",0x28aa8c,0xf0e196,0x1e5a50}};
// D040: owner-approved RR2 kits (helmet, vest, suit). Riders that share a name with an approved RR1 rider (Natasha, Slater, Rude Boy, Axle,
// Viper, Ikira, Biff) are NOT repeated here: rival_look() falls back to rr1_looks, so they keep the identical RR1 kit. Police (O'Connor,
// Marshall, Rourke, Ortega, Kelly) are deliberately absent (D041: Bunker now has the approved kit B).
inline static constexpr RivalLook rr2_looks[]={
    {"Lawson",0xdcb428,0x1e3c8c,0x28282d},{"P.E. No.1",0x28282d,0xf08214,0x464650},
    {"Buffy",0xf0f0f0,0xf068b4,0x781e5a},{"Roz",0x1ec8c8,0xf08c1e,0x28325a},
    {"Kakana",0xdc5a82,0xf0f0f0,0x1e5a8c},{"Ralph",0x5a96e6,0xebe6cd,0x3c5a28},
    {"Slug",0x82f03c,0x3c5a28,0x28282d},{"Lesley",0xf0f0f0,0x1ec8c8,0x28285a},
    {"Jorg",0xe6b428,0x1e3c8c,0x28282d},{"Sven",0xf0f0f0,0x1e50b4,0x8c1e14},
    {"Chino",0xf0d21e,0xdc2828,0x28282d},{"Warhammer",0x28282d,0xe6b428,0x5a5a64},
    {"Bunker",0x464650,0x5a6e32,0x28282d}};   // D041: kit B approved (ROM: circuit-1 racer, b13=02, never a police record)
int rr2_group_value=-1;uint64_t rr2_group_tick=UINT64_MAX;
// D044 RR3: exclusive riders (shared names fall back to the approved RR1/RR2 kits).
inline static constexpr RivalLook rr3_looks[]={
    {"Odie",0xf08214,0x1ec8c8,0x28285a},{"Lucky Luc",0xe6b428,0x28282d,0x8c1e14},{"Belladonna",0xf0f0f0,0x8c1e3c,0x462878},
    {"Scab Boy",0x5a6428,0xc8be8c,0x3c3c44},{"Slim",0xf0f0f0,0x1e50b4,0x28282d},{"Pearl",0xf0f0f0,0xbeaaf0,0x6e82a0},
    {"Rhonda",0xdc2828,0x28282d,0xf0f0f0},{"Nina",0xf068b4,0x28282d,0x6e286e},{"Bolo",0xe65a1e,0xebe6cd,0x28282d},
    {"Walleye",0xdcb428,0x28465a,0x28282d},{"Madame X",0xaaaab4,0x28282d,0xaa1e28},{"Helmut",0xdc2828,0xf0f0f0,0x28282d}};
int rr3_group_value=-1;uint64_t rr3_group_tick=UINT64_MAX;
// roles: 1 helmet, 2 vest, 3 suit, 0 other. `coded` is the same image for the coverage resolver with the role
// folded into the colour code: 0..15 line-2 nibble, 16+n helmet, 32+n vest, 48+n suit, 64+i any other CRAM index i.
struct RoleMap {int w=0,h=0;uint32_t hash=0;uint64_t tick=UINT64_MAX;std::vector<uint8_t> roles;rr::CoverageImage coded;};
std::map<const void*,RoleMap> role_maps;
struct IdentityEntry {uint32_t target;uint8_t role,nib;};
std::vector<IdentityEntry> identity_entries;
// Native (SAT) pixels of a rival: its live sprites composed into one bitmap, so helmet/vest can be told apart.
struct NativeRoles {struct Piece {unsigned slot;int cx,cy,w,h;};   // piece origin inside the composed bitmap
    int left=0,top=0,w=0,h=0;std::vector<uint8_t> roles;std::vector<Piece> pieces;};
std::map<uint64_t,NativeRoles> native_roles;   // key: (job group<<32)|actor
std::array<uint32_t,80> slot_actor{},slot_group{};unsigned native_sat=0;
// D036: sprite ownership kept independent of the geometry scene (start fade, pause, mirrors).
std::array<bool,80> id_live{};std::array<uint64_t,80> id_sig{};std::array<uint32_t,80> id_prev_owner{},id_prev_group{};uint64_t id_owner_tick=UINT64_MAX;std::unordered_map<uint64_t,std::pair<uint32_t,uint32_t>> id_memory;std::deque<Job> id_archive;
std::map<uint32_t,int> identity_blocks;std::vector<RivalLook> identity_block_looks;   // coverage palette block per rival
static constexpr size_t MAX_IDENTITY_BLOCKS=13,COVERAGE_BLOCK_BASE=256,COVERAGE_BLOCK_SIZE=128;
std::map<uint32_t,std::array<uint8_t,48>> identity_map;   // actor -> [(role-1)*16+nib] = host entry+1
bool pose_easing=false,pose_easing_logged=false;
uint64_t pose_eases=0,pose_ease_rejections=0,pose_ease_frames=0;
FILE *pose_trace=nullptr;
std::map<uint32_t,ActorCache> actor_caches;
ActorCache player_cache;   // prototype 03: the player's one rider assembly (never in actor_caches)
std::array<rr::SpriteAsset,6> player_images;unsigned player_image_next=0;   // native-bitmap poses: from/to/shown_tex only ever reference the newest three
std::vector<ActorDrawing> actor_drawings;
uint64_t actor_canvas_frames=0,actor_hold_frames=0;
uint64_t actor_source_captures=0,actor_source_assemblies=0,actor_source_only=0,actor_restored_pixels=0;
std::array<bool,80> replaced{};
std::array<bool,80> live_slots{};
std::array<bool,80> player_slots{};
std::array<bool,80> native_slots{};
uint64_t source_objects=0,retained_objects=0;
std::array<Bounds,80> bounds;
Job building;
Profile profile{};
uint64_t tick=0,serial=0,cached_tick=UINT64_MAX;
uint32_t cached_sat=0;
int cached_line=-1;
bool initialized=false,enabled=false,active=false,building_valid=false;
FILE *trace=nullptr;
FILE *object_trace=nullptr;
FILE *scenery_trace=nullptr;
FILE *traffic_trace=nullptr;
FILE *pixel_trace=nullptr;std::map<uint32_t,unsigned> vehicle_pixels;
FILE *source_audit=nullptr;
FILE *lane_trace=nullptr;
FILE *audit_trace=nullptr;
uint64_t audit_player_pixels=0,audit_player_changes=0;
uint64_t audit_player_corrupt=0,audit_player_occluded=0;
uint64_t audit_actor_pixels=0,audit_actor_missing=0,audit_blue_road=0;
uint64_t frames=0,matched=0,intermediate=0,transformed=0,resets=0;

void flush_vehicle_pixels(){
    if(pixel_trace)for(const auto& item:vehicle_pixels)std::fprintf(pixel_trace,"%llu,%06X,%u\n",(unsigned long long)tick,item.first,item.second);
    vehicle_pixels.clear();
}
void finish(){
    if(source_audit){std::fclose(source_audit);source_audit=nullptr;}
    flush_vehicle_pixels();if(pixel_trace){std::fclose(pixel_trace);pixel_trace=nullptr;}
    if(trace){std::fclose(trace);trace=nullptr;}
    if(object_trace){std::fclose(object_trace);object_trace=nullptr;}
    if(scenery_trace){std::fclose(scenery_trace);scenery_trace=nullptr;}
    if(traffic_trace){std::fclose(traffic_trace);traffic_trace=nullptr;}
    if(lane_trace){std::fclose(lane_trace);lane_trace=nullptr;}
    if(pose_trace){std::fclose(pose_trace);pose_trace=nullptr;}
    if(player_trace){std::fclose(player_trace);player_trace=nullptr;}
    if(player_jobs_trace){std::fclose(player_jobs_trace);player_jobs_trace=nullptr;}
    if(hull_trace){std::fclose(hull_trace);hull_trace=nullptr;}
    if(commit_trace){std::fclose(commit_trace);commit_trace=nullptr;}
    if(dissolve_trace){std::fclose(dissolve_trace);dissolve_trace=nullptr;}
    if(pose_easing_logged)std::fprintf(stderr,"[RR MOTORCYCLE MOTION] eases=%llu rejected=%llu eased_drawings=%llu\n",
        (unsigned long long)pose_eases,(unsigned long long)pose_ease_rejections,(unsigned long long)pose_ease_frames);
    if(audit_trace){
        std::fclose(audit_trace);audit_trace=nullptr;
        std::string path=std::string(trace_path("RR_GEOMETRY_AUDIT"))+".json";
        if(FILE *f=std::fopen(path.c_str(),"wb")){
            std::fprintf(f,"{\"player_pixels\":%llu,\"changed_player_pixels\":%llu,\"corrupted_player_pixels\":%llu,\"occluded_by_actor_pixels\":%llu,\"native_actor_pixels\":%llu,\"missing_actor_pixels\":%llu,\"blue_road_pixels\":%llu}\n",
                (unsigned long long)audit_player_pixels,(unsigned long long)audit_player_changes,
                (unsigned long long)audit_player_corrupt,(unsigned long long)audit_player_occluded,
                (unsigned long long)audit_actor_pixels,(unsigned long long)audit_actor_missing,
                (unsigned long long)audit_blue_road);
            std::fclose(f);
        }
    }
    if(const char *path=trace_path("RR_GEOMETRY_STATS"))if(FILE *file=std::fopen(path,"wb")){
        std::fprintf(file,"{\"frames\":%llu,\"matched_objects\":%llu,\"intermediate_objects\":%llu,\"transformed_pieces\":%llu,\"history_resets\":%llu,\"source_objects\":%llu,\"retained_objects\":%llu,\"terrain_pixels\":%llu,\"actor_canvas_frames\":%llu,\"actor_hold_frames\":%llu,\"lane_pixels\":%llu,\"lane_period\":%.7f,\"lane_confidence\":%.7f,\"actor_source_captures\":%llu,\"actor_source_assemblies\":%llu,\"actor_source_only\":%llu,\"actor_restored_pixels\":%llu}\n",
            (unsigned long long)frames,(unsigned long long)matched,(unsigned long long)intermediate,
            (unsigned long long)transformed,(unsigned long long)resets,(unsigned long long)source_objects,(unsigned long long)retained_objects,(unsigned long long)terrain_pixels,
            (unsigned long long)actor_canvas_frames,(unsigned long long)actor_hold_frames,(unsigned long long)lane_pixels,lane_model.period,lane_model.confidence,
            (unsigned long long)actor_source_captures,(unsigned long long)actor_source_assemblies,(unsigned long long)actor_source_only,(unsigned long long)actor_restored_pixels);std::fclose(file);
    }
    if(enabled)std::fprintf(stderr,"[RR GEOMETRY] objects=%llu intermediate=%llu transformed=%llu resets=%llu\n",
        (unsigned long long)matched,(unsigned long long)intermediate,(unsigned long long)transformed,(unsigned long long)resets);
}

uint32_t piece_pixels(const uint8_t *memory,unsigned address,unsigned size){
    uint32_t hash=2166136261u;
    for(unsigned n=0;n<size;++n)hash=hash_byte(hash,memory[(address+n)&65535]);
    return hash;
}

bool matches(const Job& job,const GVDP *v,unsigned sat,bool pixels=true){
    if(job.pieces.empty()||job.first<0||job.first+(int)job.pieces.size()>80)return false;
    for(size_t n=0;n<job.pieces.size();++n){
        const auto& piece=job.pieces[n];unsigned address=(sat+(job.first+(unsigned)n)*8)&65535;
        // The preceding object patches the link byte after this object returns.
        for(unsigned k=0;k<8;++k)if(k!=3&&v->vram[(address+k)&65535]!=piece.sat[k])return false;
        unsigned attr=(piece.sat[4]<<8)|piece.sat[5];
        unsigned cells=(((piece.sat[2]>>2)&3)+1)*((piece.sat[2]&3)+1);
        if(pixels&&piece_pixels(v->vram,(attr&2047)*32,cells*32)!=piece.pixels)return false;
    }
    return true;
}

Pose sample(const History& history,double alpha){
    const auto& a=history.previous;const auto& b=history.current;
    // Interpolate inverse projected scale (depth), then project object space.
    // No color, previous framebuffer or neighboring object participates.
    double scale=1./((1-alpha)/a.scale+alpha/b.scale);
    double x=view_center+((1-alpha)*(a.x-view_center)/a.scale+alpha*(b.x-view_center)/b.scale)*scale;
    double y=view_horizon+((1-alpha)*(a.y-view_horizon)/a.scale+alpha*(b.y-view_horizon)/b.scale)*scale;
    return {x,y,scale};
}

const rr::SpriteAsset *asset(unsigned image,unsigned flags){
    unsigned key=image*2+((flags&128)?1:0);
    auto found=assets.find(key);
    if(found==assets.end())found=assets.emplace(key,rr::decode_sprite(source_rom,source_bytes,
        asset_tables[game_index],image,(flags&128)!=0)).first;
    return found->second?&found->second:nullptr;
}

double camera_at(double time){
    if(cameras.empty())return 0;
    if(time==(double)tick)return vehicle_camera_at(cameras.back().second);
    size_t n=0;while(n+1<cameras.size()&&cameras[n+1].first<time)++n;
    const auto& a=cameras[n];
    if(time<=a.first)return a.second;
    if(n+1<cameras.size()){
        const auto& b=cameras[n+1];double t=(time-a.first)/(b.first-a.first);
        return a.second+(b.second-a.second)*t;
    }
    if(n){
        const auto& previous=cameras[n-1];double delta=a.second-previous.second;
        if(delta>=0&&delta<2&&a.first>previous.first)
            return a.second+delta*std::min(6.,time-a.first)/(a.first-previous.first);
    }
    return a.second;
}

void observe_vehicle_camera(double camera){
    if(camera<=0)return;
    if(!vehicle_cameras.empty()&&vehicle_cameras.back().second==camera)return;
    if(!vehicle_cameras.empty()&&vehicle_cameras.back().first==tick)vehicle_cameras.back().second=camera;
    else vehicle_cameras.emplace_back(tick,camera);
    while(vehicle_cameras.size()>32)vehicle_cameras.pop_front();
}
double vehicle_camera_at(double fallback){
    if(vehicle_cameras.empty())return fallback;
    if(vehicle_camera_frame==tick)return vehicle_camera_value;
    const auto& b=vehicle_cameras.back();double value=b.second,velocity=0;
    if(vehicle_cameras.size()>1){const auto& a=vehicle_cameras[vehicle_cameras.size()-2];
        double dt=(double)(b.first-a.first),delta=b.second-a.second;
        if(dt>0&&dt<=12&&delta>=0&&delta<2){
            double age=(double)(tick-b.first);velocity=delta/dt;
            // Builder hooks observe a committed native camera on slightly
            // different VBlanks. Fit the last four observations so that a
            // 4/6-frame sampling alternation is not treated as acceleration.
            double n=0,st=0,sz=0,stt=0,stz=0;
            for(auto it=vehicle_cameras.rbegin();it!=vehicle_cameras.rend()&&n<4;++it){
                double t=(double)it->first-b.first,z=it->second-b.second;
                if(t< -24||z>0)break;
                ++n;st+=t;sz+=z;stt+=t*t;stz+=t*z;
            }
            double denominator=n*stt-st*st;
            if(n>=3&&denominator>0){
                double fitted=(n*stz-st*sz)/denominator;
                if(fitted>=0&&fitted<.5){velocity=fitted;value+=std::clamp((sz-velocity*st)/n,-.25,.25);}
            }
            value+=velocity*std::min(dt,age);
            if(age>=dt)velocity=0;
        }
    }
    // Measurements can correct an earlier forecast while braking. Camera
    // motion must not reverse because a past forecast was re-interpolated.
    value=camera_filter.track(tick,value,velocity,.25,.35);
    if(vehicle_camera_frame!=UINT64_MAX&&tick>vehicle_camera_frame&&b.second>=vehicle_camera_native)
        value=std::max(value,vehicle_camera_value);
    vehicle_camera_frame=tick;vehicle_camera_native=b.second;vehicle_camera_value=value;return value;
}

Pose scene_sample(const History& h){
    // Present the latest committed pose immediately. Only the gaps between
    // native updates use bounded prediction; no five-frame replay buffer.
    double target=(double)tick;
    if(h.samples.size()<2||target<=h.samples.front().first)return h.samples.front().second;
    size_t n=1;
    while(n+1<h.samples.size()&&target>h.samples[n].first)++n;
    const auto& a=h.samples[n-1];const auto& b=h.samples[n];
    double alpha=(target-a.first)/(double)(b.first-a.first);
    // At an edge, continue confirmed depth velocity for a bounded interval;
    // clipping below decides visibility from the complete billboard.
    if(target>b.first&&h.art.projection.scenery&&!cameras.empty()){
        double ca=camera_at((double)a.first),cb=camera_at((double)b.first),ct=camera_at(target);
        alpha=cb>ca+1e-6?1.+std::max(0.,(ct-cb)/(cb-ca)):1.;
    }else alpha=std::clamp(alpha,0.,1.+(h.art.projection.actor?6.:2.)/(double)(b.first-a.first));
    if(h.seen==tick)alpha=std::min(alpha,1.+(h.art.projection.scenery||h.art.projection.actor?6.:2.)/(double)(b.first-a.first));
    History interval;interval.previous=a.second;interval.current=b.second;
    // Near the camera, inverse depth can cross zero during prediction. Do
    // not return an empty pose (or snap back to the native size). Approach a
    // finite forward bound continuously, using the same alpha for X/Y/Z.
    if(alpha>1){
        double da=1/a.second.scale,db=1/b.second.scale,closing=(alpha-1)*(da-db)/db;
        if(closing>.5){
            double bounded=.5+.25*(1-std::exp(-(closing-.5)/.25));
            alpha=1+bounded*db/(da-db);
        }
    }
    return sample(interval,alpha);
}

rr::RoadRays presentation_road(const rr::RoadRays& current,double camera){
    // World tables are camera-relative. Rebase common world nodes, while
    // keeping the current native raster's crest/mask completely unchanged.
    for(auto it=roads.rbegin();it!=roads.rend();++it){
        if(current.camera-it->rays.camera>1e-6)return rr::road_camera_basis(current,it->rays,camera);
    }
    return current;
}

// Scenery is stationary in track space. Extrapolating its screen Y around a
// fixed horizon treats a crest as vertical object motion. Reproject the native
// complete anchor against the same current road profile used by vehicles.
bool scenery_sample(History& h,Pose& pose,double& clip){
    const auto& job=h.art;const auto& p=job.projection;
    if(!p.has_road||roads.empty()||job.scale<=0)return false;
    const auto current=presentation_road(roads.back().rays,vehicle_camera_at(roads.back().rays.camera));
    double native_depth=2.+(int16_t)p.relative_z/256.;
    double camera=vehicle_camera_at(current.camera);
    // The descriptor rounds relative Z to 8.8. Reconstructing world Z from
    // that rounded value on each upload makes a stationary object oscillate.
    // Its placement hook already recovered the exact fixed track segment.
    double depth=2.+p.z-camera;
    double table_depth=2.+p.z-current.camera;
    if(native_depth<.25||depth<.25||depth>34||std::abs(current.camera-p.camera)>4)return false;
    double old_x=0,new_x=0,old_y=0,new_y=0,old_clip=0;
    if(!rr::road_lateral(p.road,native_depth,old_x)||!rr::road_lateral(current,table_depth,new_x)||
       !rr::road_point(p.road,native_depth,old_y,old_clip)||!rr::road_point(current,table_depth,new_y,clip))return false;
    double raw_old=0,raw_new=0;
    double old_position=rr::road_position_depth(p.road,native_depth),position=rr::road_position_depth(current,depth);
    double table_position=rr::road_position_depth(current,table_depth);
    if(p.road.has_height&&current.has_height&&rr::road_vehicle_height(p.road,native_depth,0,raw_old)&&
       rr::road_vehicle_height(current,table_depth,0,raw_new)){
        old_y=raw_old;new_y=raw_new;
    }
    new_y=current.horizon+(new_y-current.horizon)*table_position/position;
    // The placement high nibble determines the authored focal numerator.
    // Scale uses relative Z+2; feet use relative Z+1, as in the native code.
    double art_depth=native_depth,draw_depth=depth;
    double ax=(h.current.x-view_center)*art_depth-old_x,ay=(h.current.y-old_y)*old_position;
    double anchor_x=h.anchor_x.step(tick,ax,.5*art_depth);
    double anchor_y=h.anchor_y.step(tick,ay,.5*old_position);
    double native_focal=job.scale*art_depth,authored_focal=(p.size_offset+2)*view_focal;
    double focal=std::abs(native_focal-authored_focal)<.005?authored_focal:native_focal;
    pose={view_center+(new_x+anchor_x)/draw_depth,new_y+anchor_y/position,focal/draw_depth};
    // A crest is a screen-space mask, never a scaled part of the billboard.
    clip=std::min((double)view_bottom,clip);
    return std::isfinite(pose.x)&&std::isfinite(pose.y)&&std::isfinite(pose.scale)&&pose.scale<=32;
}

struct SceneryCopy {double x,y;unsigned attr;bool visible;};
bool scenery_copies(const Job& job,const Pose& native,std::vector<SceneryCopy>& copies){
    if(job.pieces.empty())return false;
    auto attr=[](const Piece& p){return (unsigned)((p.sat[4]<<8)|p.sat[5]);};
    auto x=[](const Piece& p){return (int)((p.sat[6]<<8|p.sat[7])&511)-128;};
    auto y=[](const Piece& p){return (int)((p.sat[0]<<8|p.sat[1])&511)-128;};
    auto width=[](const Piece& p){return (((p.sat[2]>>2)&3)+1)*8;};
    auto height=[](const Piece& p){return ((p.sat[2]&3)+1)*8;};
    auto same=[&](const Piece& a,const Piece& b){
        return a.sat[2]==b.sat[2]&&(attr(a)&2047)==(attr(b)&2047)&&a.pixels==b.pixels;
    };
    // The native scaler reuses the same pattern tiles for repeated scenery.
    // Recover the complete repeated assembly, not one billboard per job.
    size_t count=job.pieces.size(),period=count;
    for(size_t n=1;n<count;++n)if(same(job.pieces[0],job.pieces[n])){period=n;break;}
    if(count%period)return false;
    const auto& base=job.pieces[0];
    for(size_t n=0;n<count;n+=period){
        const auto& first=job.pieces[n];unsigned flip=attr(first)^attr(base);
        bool mirror_x=(flip&0x0800)!=0,mirror_y=(flip&0x1000)!=0;
        int dx=mirror_x?x(first)+x(base)+width(base):x(first)-x(base);
        int dy=mirror_y?y(first)+y(base)+height(base):y(first)-y(base);
        for(size_t k=0;k<period;++k){
            const auto& a=job.pieces[k];const auto& b=job.pieces[n+k];
            if(!same(a,b)||((attr(a)^attr(b))&0xf800)!=(flip&0xf800)||
               x(b)!=(mirror_x?dx-x(a)-width(a):x(a)+dx)||
               y(b)!=(mirror_y?dy-y(a)-height(a):y(a)+dy))return false;
        }
        bool visible=false;
        for(size_t k=0;k<period;++k){const auto& p=job.pieces[n+k];
            visible|=x(p)+width(p)>left_edge()&&x(p)<right_edge()&&y(p)+height(p)>view_top&&y(p)<std::min((double)view_bottom,job.clip);}
        copies.push_back({mirror_x?dx-native.x:native.x+dx,
                          mirror_y?dy-native.y:native.y+dy,attr(first),visible});
    }
    return true;
}

bool add_billboards(History& h,bool retained){
    const auto& job=h.art;
    // Animated riders/traffic are composed and recolored by the software
    // scaler. Their committed SAT pieces are the complete current pose.
    if(!job.projection.scenery)return false;
    const auto *first=asset(job.image,job.flags);
    const auto *second=job.second?asset(job.second,job.flags):nullptr;
    if(!first||h.samples.empty())return false;
    auto visible_art=[](const rr::SpriteAsset *art){return art&&std::any_of(art->pixels.begin(),art->pixels.end(),[](uint8_t p){return p!=0;});};
    if(!visible_art(first)&&!visible_art(second))return false;
    Pose pose=scene_sample(h);if(pose.scale<=0||pose.scale>32)return false;
    std::vector<SceneryCopy> copies;
    if(!scenery_copies(job,h.current,copies))return false;
    // Preserve the software scaler's minimum-pixel decisions at the horizon.
    if(!coverage_enabled&&!retained&&(first->width*pose.scale<1||first->height*pose.scale<1))return false;
    // Native per-object hill clipping is descriptor+6, distinct from the HUD.
    double clip=job.clip>=view_bottom?(double)view_bottom:std::min((double)view_bottom,pose.y+(job.clip-job.y)*pose.scale/job.scale);
    if(job.projection.has_road&&!scenery_sample(h,pose,clip))return false;
    auto append=[&](const rr::SpriteAsset *art,const Pose& instance,unsigned drawing_flags,unsigned attr){
        if(!art)return;
        int ax=(drawing_flags&0x8000)?art->width-art->anchor_x:art->anchor_x;
        int ay=(drawing_flags&0x4000)?art->height-art->anchor_y:art->anchor_y;
        double left=instance.x-ax*pose.scale,top=instance.y-ay*pose.scale;
        double right=left+art->width*pose.scale,bottom=top+art->height*pose.scale;
        if(right<=left_edge()||left>=right_edge()||bottom<=view_top||top>=view_bottom)return;
        // A rectangle alone is not a rendered replacement. Thin minified art
        // or hill clipping can leave every sampled texel transparent.
        bool opaque=coverage_enabled;
        int y0=std::max(view_top,(int)std::ceil(top-.5)),y1=std::min(view_bottom,(int)std::ceil(std::min(bottom,clip)-.5));
        int x0=std::max(left_edge(),(int)std::ceil(left-.5)),x1=std::min(right_edge(),(int)std::ceil(right-.5));
        for(int y=y0;y<y1&&!opaque;++y){
            int w=(int)std::floor((y+.5-top)/pose.scale);if(w<0||w>=art->height)continue;
            if(drawing_flags&0x4000)w=art->height-1-w;
            for(int x=x0;x<x1;++x){int u=(int)std::floor((x+.5-left)/pose.scale);
                if(u<0||u>=art->width)continue;if(drawing_flags&0x8000)u=art->width-1-u;
                if(art->pixels[w*art->width+u]){opaque=true;break;}}
        }
        if(!opaque)return;
        billboards.push_back({art,instance,(int)drawing_flags,(int)((attr>>13)&3),(int)(attr>>15),job.depth,clip,(unsigned)job.first+1});
    };
    size_t before=billboards.size();double ratio=pose.scale/job.scale;
    for(const auto& copy:copies){
        size_t copy_start=billboards.size();
        Pose instance{pose.x+(copy.x-h.current.x)*ratio,pose.y+(copy.y-h.current.y)*ratio,pose.scale};
        unsigned drawing_flags=(job.flags&~0xc000u)|((copy.attr&0x0800)?0x8000u:0u)|((copy.attr&0x1000)?0x4000u:0u);
        append(first,instance,drawing_flags,copy.attr);append(second,instance,drawing_flags,copy.attr);
        if(!coverage_enabled&&!retained&&copy.visible&&billboards.size()==copy_start){billboards.resize(before);return false;}
    }
    if(object_trace)for(size_t n=before;n<billboards.size();++n){const auto& b=billboards[n];
        int ax=(b.flags&0x8000)?b.art->width-b.art->anchor_x:b.art->anchor_x;
        int ay=(b.flags&0x4000)?b.art->height-b.art->anchor_y:b.art->anchor_y;
        double left=b.pose.x-ax*b.pose.scale,top=b.pose.y-ay*b.pose.scale;
        std::fprintf(object_trace,"%llu,%016llX,%u,%.6f,%.6f,%.6f,%.6f,%d,%llu,%.7f,%.7f,%.7f,%llu,%.7f,%.4f,%.7f,%.7f\n",
            (unsigned long long)tick,(unsigned long long)job.key,job.image,left,top,
            left+b.art->width*b.pose.scale,top+b.art->height*b.pose.scale,retained?1:0,(unsigned long long)(tick-h.seen),
            h.current.x,h.current.y,job.scale,(unsigned long long)job.projection.tick,2.+(int16_t)job.projection.relative_z/256.,
            job.projection.size_offset,job.projection.camera,vehicle_camera_at(job.projection.camera));}
    if(billboards.size()!=before){++source_objects;if(retained)++retained_objects;}
    return coverage_enabled||billboards.size()!=before;
}

void scenery_audit(const GVDP *v,const Job& job,const Pose& native,bool replacement){
    if(!scenery_trace||!job.projection.scenery)return;
    auto first=asset(job.image,job.flags),second=job.second?asset(job.second,job.flags):nullptr;
    std::vector<SceneryCopy> copies;scenery_copies(job,native,copies);
    unsigned pixels=0,missing=0;int left=9999,top=9999,right=-9999,bottom=-9999;
    auto covered=[&](const rr::SpriteAsset* art,int x,int y,const SceneryCopy& copy){
        if(!art)return false;
        bool flip_x=(copy.attr&0x0800)!=0,flip_y=(copy.attr&0x1000)!=0;
        int ax=flip_x?art->width-art->anchor_x:art->anchor_x;
        int ay=flip_y?art->height-art->anchor_y:art->anchor_y;
        int u=(int)std::floor((x+.5-copy.x)/native.scale+ax);
        int w=(int)std::floor((y+.5-copy.y)/native.scale+ay);
        if(u<0||u>=art->width||w<0||w>=art->height||y+.5>=job.clip)return false;
        if(flip_x)u=art->width-1-u;if(flip_y)w=art->height-1-w;
        return art->pixels[w*art->width+u]!=0;
    };
    for(const auto& p:job.pieces){
        unsigned attr=(p.sat[4]<<8)|p.sat[5];
        int sx=((p.sat[6]<<8|p.sat[7])&511)-128,sy=((p.sat[0]<<8|p.sat[1])&511)-128;
        int width=(((p.sat[2]>>2)&3)+1)*8,height=((p.sat[2]&3)+1)*8;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            if(sx+x<view_left||sx+x>=view_right||sy+y<view_top||sy+y>=view_bottom)continue;
            int u=attr&0x0800?width-1-x:x,w=attr&0x1000?height-1-y:y;
            unsigned tile=((attr&2047)+(u>>3)*(height/8)+(w>>3))&2047;
            unsigned byte=v->vram[(tile*32+(w&7)*4+(u&7)/2)&65535];
            if(!((byte>>((u&1)?0:4))&15))continue;
            bool visible=false;for(const auto& copy:copies)visible|=covered(first,sx+x,sy+y,copy)||covered(second,sx+x,sy+y,copy);
            ++pixels;missing+=!visible;
            left=std::min(left,sx+x);top=std::min(top,sy+y);right=std::max(right,sx+x);bottom=std::max(bottom,sy+y);
        }
    }
    std::fprintf(scenery_trace,"%llu,%llu,%u,%u,%.5f,%.5f,%.6f,%.3f,%u,%u,%d,%d,%d,%d,%04X,%04X,",(unsigned long long)tick,
        (unsigned long long)job.serial,job.image,job.second,native.x,native.y,native.scale,job.clip,pixels,missing,left,top,right,bottom,job.flags,
        job.pieces.empty()?0:((job.pieces.front().sat[4]<<8)|job.pieces.front().sat[5]));
    for(const auto& p:job.pieces)for(unsigned byte:p.sat)std::fprintf(scenery_trace,"%02X",byte);
    std::fprintf(scenery_trace,",%u,%u\n",(unsigned)copies.size(),replacement?1:0);
}

int unwrap_x(int x,double anchor){
    while(x<anchor-256)x+=512;while(x>anchor+256)x-=512;return x;
}

int road_scroll(const GVDP *v,int line,unsigned side){
    unsigned table=(v->reg[13]&63)<<10;
    int raw=(int16_t)word(v->vram,table+line*4+side*2);
    if(game_index<0||(!view_id&&game_index>1)||line<view_top+8||v->reg[2]!=0x10||
       (v->reg[16]&3)!=3||(v->reg[11]&3)!=3)return raw;
    // Verified native bitmap clamps, A then B. Both extremes of both halves
    // must share the same treatment; fixing just A shifts the road center.
    constexpr int limits[2][2][2]={{{320,672},{-352,0}},{{-1024,-352},{672,1056}}};
    // Split raster builders exchange the A/B masks in the lower camera.
    constexpr int split_limits[2][2][2]={{{-344,0},{320,640}},{{320,704},{-320,0}}};
    int minimum=view_id?split_limits[view_id-1][side][0]:limits[game_index][side][0];
    int maximum=view_id?split_limits[view_id-1][side][1]:limits[game_index][side][1];
    if(raw!=minimum&&raw!=maximum)return raw;
    // High crests/strong curves can clamp above the old line-72 cutoff.
    // Confirm authored ground there so parallax/sky rows never participate.
    if(line<72)for(int n=0;n<8;++n){
        int color=plane_texel(v,side?0x6000:0x4000,(view_id?0:480)+n,line+v->vsram[side]);
        if(color!=58&&color!=59)return raw;
    }
    std::array<int,152> edge{};
    for(unsigned y=0;y<(unsigned)(view_bottom-view_top);++y)
        edge[y]=(int16_t)word(v->vram,table+(y+view_top)*4+side*2);
    return rr::extend_clipped_edge(edge,(unsigned)(line-view_top),raw,raw==maximum);
}

int paused_road_scroll(void*,const GVDP *v,int line,int *values){
    if(!row_inside(line)||(v->reg[12]&6)==6||(v->reg[11]&4))return 0;
    unsigned table=(v->reg[13]&63)<<10;
    for(unsigned side=0;side<2;++side){
        int raw=(int16_t)word(v->vram,table+line*4+side*2);
        values[side]=(values[side]+road_scroll(v,line,side)-raw)&1023;
    }
    return 1; // current raster only: no temporal prediction, material or actors
}

void find_markings(const GVDP *v,unsigned sat,const std::array<bool,80>& linked){
    marking_slots.fill(false);native_strokes.fill(LaneStroke{});unsigned count=0;
    std::array<double,240> centers{};
    for(int y=(int)view_horizon;y<view_bottom;++y)
        centers[y]=(road_scroll(v,y,0)+road_scroll(v,y,1))*.5;
    std::array<bool,80> object_slots{};
    if(game_index<2)for(const auto& job:pending){
        if(tick<job.tick||tick-job.tick>40||!matches(job,v,sat,false))continue;
        for(unsigned n=0;n<job.pieces.size();++n)
            if(linked[job.first+n])object_slots[job.first+n]=true;
    }
    for(unsigned slot=0;slot<80;++slot){
        if(!linked[slot]||object_slots[slot])continue;
        unsigned at=(sat+slot*8)&65535,attr=word(v->vram,at+4);
        if((attr&0xe000)!=0x6000)continue; // road paint: low-priority palette 3
        int sx=(int)(word(v->vram,at+6)&511)-128,sy=(int)(word(v->vram,at)&511)-128;
        unsigned size=v->vram[(at+2)&65535];int w=(((size>>2)&3)+1)*8,h=((size&3)+1)*8;
        if(sy>=view_bottom||sy+h<view_horizon)continue;
        bool valid=true;unsigned visible=0;
        std::vector<std::pair<int,int>> pixels;
        for(int y=0;y<h&&valid;++y)for(int x=0;x<w;++x){
            int u=(attr&2048)?w-1-x:x,t=(attr&4096)?h-1-y:y;
            unsigned tile=((attr&2047)+(u/8)*(h/8)+t/8)&2047;
            unsigned byte=v->vram[(tile*32+(t&7)*4+(u&7)/2)&65535];
            unsigned nib=(byte>>((u&1)?0:4))&15;if(!nib)continue;
            if(nib!=12){valid=false;break;}
            int py=sy+y;if(py<view_horizon||py>=view_bottom)continue;
            double center=centers[py];
            int px=unwrap_x(sx+x,center);
            // Paint pieces can be separated by a large offset when the
            // native edge is clamped or its SAT update spans a frame. Match
            // the complete monochrome paint set, excluding scaled objects.
            if(game_index==2&&std::abs(px+.5-center)>12){valid=false;break;}
            pixels.emplace_back(px,py);++visible;
        }
        if(!valid||visible<2)continue;
        marking_slots[slot]=true;++count;
        for(auto pixel:pixels){auto& row=native_strokes[pixel.second];
            row.left=std::min(row.left,pixel.first);row.right=std::max(row.right,pixel.first);}
    }
    markings_ready=count!=0;
}

uint64_t palette_key(unsigned image,unsigned second,unsigned flags){return ((uint64_t)image<<32)|((uint64_t)second<<16)|(flags&0xc080);}
const SourcePacket *source_packet(uint32_t actor,uint64_t epoch){
    auto it=actor_sources.find(actor);if(it==actor_sources.end())return nullptr;
    for(auto p=it->second.rbegin();p!=it->second.rend();++p)if(p->epoch==epoch)return &*p;
    return nullptr;
}
static bool wide_scene(){const char *wide=std::getenv("RR_WIDE");return wide&&*wide=='1';}
bool edge_recovery(){const char *mode=std::getenv("RR_EDGE_RECOVERY");return (view_id||wide_scene())&&(!mode||*mode!='0');}
bool arcade_motion(){const char *mode=std::getenv("RR_ARCADE_MOTION");return !mode||*mode!='0';}
bool rear_view(const SourcePacket& packet){return packet.camera>0&&packet.world<packet.camera;}
bool rigid_vehicle(const SourcePacket& packet){
    if(packet.parts.empty())return false;
    const auto& root=packet.parts.front();
    for(const auto& p:packet.parts){
        double dx=std::remainder(p.x-root.x,512.);
        if(std::abs(p.scale-root.scale)>1./16384.||std::abs(p.y-root.y)>std::max(8.,8.*root.scale)||
           std::abs(dx)>std::max(32.,48.*root.scale))return false;
    }
    return true;
}
double smooth_vehicle_lane(ActorCache& cache,double target,double velocity,double depth){
    if(cache.lane_frame==tick)return cache.lane_position;
    double dt=cache.lane_frame==UINT64_MAX?0:(double)(tick-cache.lane_frame);
    if(dt<=0||dt>12||std::abs(target-cache.lane_position)>64.*depth){
        cache.lane_position=target;cache.lane_velocity=velocity;
    }else{
        // A critically damped correction follows the live target and its
        // velocity. It does not delay artwork or replay old framebuffers.
        constexpr double omega=1.25;
        double error=cache.lane_position-(target-velocity*dt);
        double relative=cache.lane_velocity-velocity;
        double step=(relative+omega*error)*dt,decay=std::exp(-omega*dt);
        cache.lane_position=target+(error+step)*decay;
        cache.lane_velocity=velocity+(relative-omega*step)*decay;
        // Limit presentation separation from the measured/predicted lane to
        // eight screen pixels, including close passes. Teleports reset above.
        cache.lane_position=std::clamp(cache.lane_position,target-8.*depth,target+8.*depth);
    }
    cache.lane_frame=tick;return cache.lane_position;
}
bool read_road(int game,const uint8_t *ram,rr::RoadRays& road){
    constexpr unsigned pointers[]={0xd77c,0x1ce4,0x1cdc};
    unsigned at=view_id?view.road_y:game==0?pointers[0]:(longword(ram,pointers[game])&65535);
    road.camera=longword(ram,view.camera)/65536.;road.has_x=road.has_height=false;
    if(at<=65536-64){
        for(unsigned i=0;i<32;++i)road.y[i]=(int16_t)word(ram,at+i*2);
        constexpr unsigned xy_pointers[]={0x310a,0x1ce8,0x1ce0};
        unsigned xy=view_id?(0xff0000u|view.road_xy):game==0?0xff310a:longword(ram,xy_pointers[game]);
        if(xy>=0xff0000&&xy<=0xffff80){
            for(unsigned i=0;i<32;++i){road.x[i]=(int16_t)word(ram,(xy&65535)+i*4);
                road.world_y[i]=(int16_t)word(ram,(xy&65535)+i*4+2);}
            road.has_x=true;
            road.has_height=std::any_of(road.world_y.begin(),road.world_y.end(),[](double y){return y!=0;});
        }
        road.horizon=view_id?view_horizon:game==0?76:(int16_t)word(ram,game==1?0xf2bc:0x59a2);
        road.clip=view_bottom;road.height_scale=view_id&&game==1?.625:1.;road.position_bias=1;
        return road.y[0]>32&&road.y[0]<=512&&road.y[31]>=-128&&road.y[31]<=224&&road.y[0]!=road.y[31];
    }
    return false;
}
void stamp_actor(SourcePacket& packet,int game,uint32_t actor,const uint8_t *ram){
    packet.world=longword(ram,(actor&65535)+4)/65536.;
    packet.camera=longword(ram,view.camera)/65536.;
    observe_vehicle_camera(packet.camera);
    packet.lateral=(int16_t)word(ram,(actor&65535)+48);packet.alive=(ram[((actor&65535)+104)&65535]&0x40)!=0;
    packet.render_state=(ram[((actor&65535)+104)&65535]<<8)|ram[((actor&65535)+110)&65535];
    packet.height=(int16_t)word(ram,(actor&65535)+50);
    packet.has_road=read_road(game,ram,packet.road);
}
PoseLine pose_line(const rr::SpriteAsset& art,bool flip){
    // Least-squares line through the centroid of every opaque source row.
    double n=0,sh=0,sc=0,shh=0,shc=0;
    for(int y=0;y<art.height;++y){double count=0,sum=0;
        for(int x=0;x<art.width;++x)if(art.pixels[(size_t)y*art.width+x]){++count;sum+=x+.5;}
        if(!count)continue;
        double h=art.anchor_y-(y+.5),c=sum/count-art.anchor_x;
        ++n;sh+=h;sc+=c;shh+=h*h;shc+=h*c;
    }
    double denominator=n*shh-sh*sh;
    if(n<8||denominator<=0)return {};
    double slope=(n*shc-sh*sc)/denominator,offset=(sc-slope*sh)/n;
    return {flip?-offset:offset,flip?-slope:slope,(double)std::max(art.anchor_y,1),true};
}
// Remaining share of a pose residual. It reaches exactly zero `duration-1`
// frames after the native commit; the artwork itself is never delayed.
double pose_ease_factor(const PoseEase& e,uint64_t at) const {
    if(!e.active||at<e.start)return 0;
    return std::max(0.,1.-((double)(at-e.start)+1.)/e.duration);
}
// Called exactly where a newly committed native assembly replaces the cached
// artwork. Classic mode (and every unqualified case) leaves no residual.
PoseShape pose_shape(const rr::SpriteAsset& art,bool flip){
    PoseShape s;int top=-1;
    for(int y=0;y<art.height&&top<0;++y)for(int x=0;x<art.width;++x)if(art.pixels[(size_t)y*art.width+x]){top=y;break;}
    if(top<0||art.anchor_y-top<8)return s;
    s.height=art.anchor_y-top;int rows=0;
    for(int k=0;k<POSE_ROWS;++k){
        int y=(int)std::floor(art.anchor_y-(k+.5)/POSE_ROWS*s.height),l=-1,r=-1;
        if(y>=0&&y<art.height)for(int x=0;x<art.width;++x)if(art.pixels[(size_t)y*art.width+x]){if(l<0)l=x;r=x+1;}
        if(l<0){s.left[k]=1;s.right[k]=0;continue;}
        float a=(float)(l-art.anchor_x),b=(float)(r-art.anchor_x);
        s.left[k]=flip?-b:a;s.right[k]=flip?-a:b;++rows;
    }
    s.valid=rows>=POSE_ROWS/2;return s;
}
static double pose_curve(double u){return u*u*(3.-2.*u);}
static PoseShape pose_between(const PoseShape& a,const PoseShape& b,double s){
    PoseShape out;out.valid=true;out.height=a.height+(b.height-a.height)*s;
    for(int k=0;k<POSE_ROWS;++k){bool ea=a.right[k]<=a.left[k],eb=b.right[k]<=b.left[k];
        if(ea&&eb){out.left[k]=1;out.right[k]=0;}
        else if(ea){out.left[k]=b.left[k];out.right[k]=b.right[k];}
        else if(eb){out.left[k]=a.left[k];out.right[k]=a.right[k];}
        else{out.left[k]=(float)(a.left[k]+(b.left[k]-a.left[k])*s);out.right[k]=(float)(a.right[k]+(b.right[k]-a.right[k])*s);}
    }
    return out;
}
// Largest hull-edge distance between two silhouettes, in source pixels.
static double pose_distance(const PoseShape& a,const PoseShape& b){
    double reach=0;
    for(int k=0;k<POSE_ROWS;++k)if(a.right[k]>a.left[k]&&b.right[k]>b.left[k])
        reach=std::max(reach,(double)std::max(std::abs(a.left[k]-b.left[k]),std::abs(a.right[k]-b.right[k])));
    return reach;
}
double pose_progress(const PoseMorph& m,uint64_t at) const {return at<m.begin?0.:((double)(at-m.begin)+1.)/m.duration;}
// ---- Prototype 03 ----------------------------------------------------------
// Prototype 02 restarts a smoothstep at every native swap, so the on-screen
// velocity drops to zero every ~5 frames (a 12 Hz judder at normal speed).
// Here the shown hull is a critically damped follower of the newest native
// hull: position AND velocity stay continuous, nothing is ever blended, and
// each frame is still ONE source image resampled with nearest texels.
static PoseShape pose_fill(const PoseShape& in){
    PoseShape out=in;int first=-1;
    for(int k=0;k<POSE_ROWS&&first<0;++k)if(in.right[k]>in.left[k])first=k;
    if(first<0)return out;
    for(int k=0;k<first;++k){out.left[k]=in.left[first];out.right[k]=in.right[first];}
    for(int k=first+1;k<POSE_ROWS;++k)if(!(in.right[k]>in.left[k])){out.left[k]=out.left[k-1];out.right[k]=out.right[k-1];}
    return out;
}
static double pose_gap(const PoseShape& a,const PoseShape& b){return std::max(pose_distance(a,b),std::abs(a.height-b.height));}
// Width change per row (a minus b) on rows drawn in both: peak, median and the largest jump between
// neighbouring rows ("kink"). A lean changes widths smoothly; a limb sticking out does not.
static void pose_width_profile(const PoseShape& a,const PoseShape& b,double& peak,double& median,double& kink){
    std::vector<double> d;double previous=0;bool have=false;peak=kink=0;
    for(int k=0;k<POSE_ROWS;++k){
        if(!(a.right[k]>a.left[k]&&b.right[k]>b.left[k])){have=false;continue;}
        double w=(a.right[k]-a.left[k])-(b.right[k]-b.left[k]);d.push_back(std::abs(w));peak=std::max(peak,std::abs(w));
        if(have)kink=std::max(kink,std::abs(w-previous));
        previous=w;have=true;
    }
    if(d.empty()){median=0;return;}
    std::sort(d.begin(),d.end());median=d[d.size()/2];
}
// A limb moving (punch, kick, arm back) changes the width of a FEW rows by a lot; a lean or a
// posture step changes many rows a little. Row-wise reconstruction would stretch the torso along
// with a limb, so those swaps stay native. Measured on the replay: every punch transition has
// peak >= 16 px with a median <= 2 px; lean steps spread the change over many rows.
// The player is large on screen (strict); rivals are small and far (tolerant).
double pose_limb_peak=13.,pose_limb_peak_rival=16.;FILE *commit_trace=nullptr;
void commit_follow(PoseMorph& m,const PoseTexture& next,bool known,bool continuous,double omega,int kind=1){
    m.omega=omega;
    auto rest=[&](const PoseTexture& t){
        m.to=m.from=m.shown_tex=t;m.shown=pose_fill(t.shape);m.goal=m.shown;m.lv.fill(0);m.rv.fill(0);m.hv=0;};
    if(!known||!continuous){m.active=false;if(known)rest(next);}
    else if(next.image!=m.to.image||next.flags!=m.to.flags){
        const PoseShape shown=m.active?m.shown:pose_fill(m.to.shape);
        const PoseTexture displayed=m.active?m.shown_tex:m.to;
        double reach=pose_gap(shown,next.shape),peak,median,kink;
        pose_width_profile(m.to.shape,next.shape,peak,median,kink);
        // Not a lean/posture step: a limb moving, or a tumble (many rows change a lot, or the
        // silhouette gets much shorter/taller). Measured on the replay: lean steps have a peak
        // width change <= 13 px and a height change <= 6 px; punches/kicks are local; crashes
        // reach peak >= 20 px or height changes of 10-16 px.
        bool limb=(peak>=(kind==0?pose_limb_peak:pose_limb_peak_rival)&&peak>=4*(median+1))||peak>=20.||
            std::abs(m.to.shape.height-next.shape.height)>8.||(kind==0&&reach>14.);
        if(commit_trace)std::fprintf(commit_trace,"%llu,%d,%u,%u,%.3f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f\n",
            (unsigned long long)tick,kind,m.to.image,next.image,reach,peak,median,kink,limb?1:0,
            std::abs(m.to.shape.height-next.shape.height),m.to.shape.height,next.shape.height);
        // Beyond this it is another kind of image (crash, attack reach), not
        // a lean or posture step: keep the native swap.
        if(reach>.05&&reach<=20.&&!limb){
            if(!m.active){m.shown=shown;m.lv.fill(0);m.rv.fill(0);m.hv=0;}
            m.from=displayed;m.to=next;m.goal=pose_fill(next.shape);
            m.active=true;m.stepped=tick-1;m.begin=tick;m.built=UINT64_MAX;++pose_eases;
        }else{m.active=false;if(reach>20.||limb)++pose_ease_rejections;rest(next);}
    }
    m.known=known;m.committed=tick;
}
// Exact solution of x''=-2w x'-w^2 (x-goal) over one frame: stable for any w.
void follow_advance(PoseMorph& m){
    uint64_t steps=m.stepped==UINT64_MAX?1:(tick>m.stepped?tick-m.stepped:0);steps=std::min<uint64_t>(steps,6);
    const double w=m.omega,e=std::exp(-w);
    auto go=[&](float& x,float target,float& v){double d=x-target,c=v+w*d;x=(float)(target+(d+c)*e);v=(float)((v-w*c)*e);};
    for(uint64_t n=0;n<steps;++n){
        for(int k=0;k<POSE_ROWS;++k){go(m.shown.left[k],m.goal.left[k],m.lv[k]);go(m.shown.right[k],m.goal.right[k],m.rv[k]);}
        float h=(float)m.shown.height,hv=(float)m.hv;go(h,(float)m.goal.height,hv);m.shown.height=h;m.hv=hv;
    }
    m.stepped=tick;
}
// Same raster rule as prototype 02: every displayed row keeps its own texels
// and is displaced by the hull difference; no blending, no second image.
bool render_follow(PoseMorph& m,const PoseTexture& t,const PoseShape& shown,const Pose& root){
    const auto& art=*t.art;double height=shown.height,source_height=t.shape.height;
    int rows=(int)std::ceil(height-1e-9);
    if(rows<4||rows>art.height+8)return false;
    // Rows at/below the ground anchor (a wheel's last pixel row) are not part
    // of the hull: they ride along with the lowest hull row, unscaled.
    int below=std::max(0,art.height-std::max(art.anchor_y,0));
    struct Span {int y;double l,r,sl,sr;};
    std::vector<Span> spans((size_t)(rows+below));double lo=1e9,hi=-1e9;
    for(int j=0;j<rows+below;++j){auto& row=spans[j];row.y=-1;
        double hn=j<rows?(rows-j-.5)/height:0.;if(hn>=1)continue;
        int y=j<rows?(int)std::floor(art.anchor_y-hn*source_height):art.anchor_y+(j-rows),l=-1,r=-1;
        if(y<0||y>=art.height)continue;
        for(int x=0;x<art.width;++x)if(art.pixels[(size_t)y*art.width+x]){if(l<0)l=x;r=x+1;}
        if(l<0)continue;
        double sl=l-art.anchor_x,sr=r-art.anchor_x;if(t.flip){double q=-sr;sr=-sl;sl=q;}
        int k=std::min(POSE_ROWS-1,(int)(hn*POSE_ROWS));
        double tl=t.shape.left[k],tr=t.shape.right[k];if(tr<=tl){tl=sl;tr=sr;}
        row.sl=sl;row.sr=sr;row.l=sl+(shown.left[k]-tl);row.r=sr+(shown.right[k]-tr);
        if(row.r-row.l<1)continue;
        row.y=y;lo=std::min(lo,row.l);hi=std::max(hi,row.r);
    }
    if(hi<=lo)return false;
    int left=(int)std::floor(lo),width=(int)std::ceil(hi)-left;
    auto& f=m.frame;f.width=width;f.height=rows+below;f.anchor_x=-left;f.anchor_y=rows;f.palette=art.palette;
    f.pixels.assign((size_t)width*(rows+below),0);
    for(int j=0;j<rows+below;++j){const auto& row=spans[j];if(row.y<0)continue;
        for(int i=0;i<width;++i){double xa=left+i+.5;if(xa<row.l||xa>=row.r)continue;
            double us=row.sl+(xa-row.l)*(row.sr-row.sl)/(row.r-row.l);
            int x=(int)std::floor(t.flip?art.anchor_x-us:art.anchor_x+us);if(x<0||x>=art.width)continue;
            f.pixels[(size_t)j*width+i]=art.pixels[(size_t)row.y*art.width+x];
        }
    }
    f.coverage.build(width,rows+below,f.pixels.data());
    m.part=ActorArt{&f,SourcePart{root.x,root.y,root.scale,152,t.image,0,t.flags,0},t.attr&~0x1800u,t.base,false};
    return true;
}
// One reconstructed in-between image for this frame. False = show the native pose.
bool build_follow(PoseMorph& m,const Pose& root,bool force=false){
    if(!m.active&&!force)return false;
    if(m.built==tick)return true;
    if(m.active){
        follow_advance(m);
        double gap=pose_gap(m.shown,m.to.shape);
        // Converged to within the pixel grid: hand over to the native image.
        if(gap<.3&&std::abs(m.hv)<.2){m.active=false;m.shown_tex=m.to;m.shown=pose_fill(m.to.shape);if(!force)return false;}
    }
    const PoseTexture *t=&m.to;
    if(m.active){
        // Show the texture whose own silhouette is nearer to the hull on screen.
        t=pose_gap(m.shown,m.to.shape)<=pose_gap(m.shown,m.from.shape)?&m.to:&m.from;m.shown_tex=*t;
    }
    if(!render_follow(m,*t,m.active?m.shown:pose_fill(m.to.shape),root)){m.active=false;return false;}
    m.built=tick;return true;
}
// Rectangle (screen pixels) holding every image this follower can show now.
void follow_box(const PoseMorph& m,const Pose& root,double box[4]){
    box[0]=box[1]=1e9;box[2]=box[3]=-1e9;
    auto add=[&](const rr::SpriteAsset& a,bool flip){
        int ax=flip?a.width-a.anchor_x:a.anchor_x;double l=root.x-ax*root.scale,t=root.y-a.anchor_y*root.scale;
        box[0]=std::min(box[0],l);box[1]=std::min(box[1],t);
        box[2]=std::max(box[2],l+a.width*root.scale);box[3]=std::max(box[3],t+a.height*root.scale);};
    if(m.to.art)add(*m.to.art,m.to.flip);if(m.from.art)add(*m.from.art,m.from.flip);
    const auto& f=m.frame;if(f.width>0)add(f,false);
}
// Diagnostic only (RR_POSE_HULL_TRACE): silhouette edges at three heights of the NATIVE pose
// (what classic shows) and of the pose actually shown, for the smoothness measurement.
void trace_hull(uint32_t actor,double scale,const PoseShape& native,const PoseShape& shown){
    if(!hull_trace)return;
    std::fprintf(hull_trace,"%llu,%06X,%.4f,%.3f,%.3f",(unsigned long long)tick,actor,scale,native.height,shown.height);
    for(const PoseShape *s:{&native,&shown})for(int k:{10,32,54})
        std::fprintf(hull_trace,",%.3f,%.3f",s->right[k]>s->left[k]?s->left[k]:0.f,s->right[k]>s->left[k]?s->right[k]:0.f);
    std::fputc('\n',hull_trace);
}
// The player's rider+bike: the SAME follower on the one native assembly. The
// native pieces stay authoritative for position (job x/y), so the bike never
// leaves the ground or the road raster; only the lean/posture silhouette is
// reconstructed. Returns true while a reconstructed frame replaces them.
bool track_player(const GVDP *v,const Job& job,double x){
    auto& cache=player_cache;auto& m=cache.morph;
    cache.root=Pose{x,job.y,1.};
    if(job.serial!=player_serial){
        player_serial=job.serial;
        PoseTexture next;bool known=false;
        // The pose is the committed NATIVE bitmap: every piece of the assembly (bike,
        // rider and the `second` overlay image of a lean) with its own palette line.
        // Per-pixel palette index/priority are copied, never recomputed.
        ActorCanvas canvas;const std::vector<const Job*> one{&job};
        if(!(job.flags&0x4000)&&compose_actor(v,one,cache.root,canvas)&&canvas.width>0&&canvas.height>0){
            unsigned high=2;bool uniform=true;
            for(size_t n=0;n<canvas.opaque.size();++n)if(canvas.opaque[n]){
                if(high==2)high=canvas.high[n];else uniform&=canvas.high[n]==high;}
            if(uniform&&high<2){
                auto& a=player_images[player_image_next++%player_images.size()];
                a.width=canvas.width;a.height=canvas.height;a.palette=0;
                a.anchor_x=(int)std::lround(x-canvas.x);a.anchor_y=(int)std::lround(job.y-canvas.y);
                a.pixels.assign((size_t)canvas.width*canvas.height,0);
                for(size_t n=0;n<canvas.opaque.size();++n)if(canvas.opaque[n])a.pixels[n]=canvas.index[n];
                next.art=&a;next.image=job.image<<16|job.second;next.flags=job.flags&0xc080;
                next.attr=high?0x8000u:0u;next.base=0;next.flip=false;
                next.shape=pose_shape(a,false);known=next.shape.valid;
            }
        }
        bool continuous=known&&m.known&&tick>m.committed&&tick-m.committed<=12;
        commit_follow(m,next,known,continuous,pose_follow_player,0);
    }
    bool shown=build_follow(m,cache.root,pose_player_force&&m.known);
    if(hull_trace&&m.known)trace_hull((uint32_t)view.player,1.,m.to.shape,m.active?m.shown:m.to.shape);
    if(player_trace&&(shown||m.active)){double box[4];follow_box(m,cache.root,box);
        std::fprintf(player_trace,"%llu,%06X,%llu,%u,%.4f,%.4f,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f,%.2f,%.2f\n",
            (unsigned long long)tick,(unsigned)view.player,(unsigned long long)job.serial,m.to.image,m.omega,
            pose_gap(m.shown,m.to.shape),pose_gap(m.shown,m.to.shape),0.,1.,std::abs(m.shown.height-m.to.shape.height),
            box[0],box[1],box[2],box[3]);}
    return shown;
}
// Prototype 02 commit: the native swap tick starts a silhouette morph from
// what was displayed on the previous frame to the newly committed image.
void commit_morph(ActorCache& cache,const SourcePacket *packet){
    auto& m=cache.morph;PoseTexture next;bool known=false;
    // Only a pose that classic ALREADY draws from its one qualified source
    // image. In-betweens then use exactly the same texels, palette and raster
    // as the frames around them. A committed native bitmap (4:3, unsupported
    // recolour) keeps the native swap: mixing the two rasters would pop.
    if(packet&&packet->parts.size()==1&&!packet->parts[0].second&&!(packet->parts[0].flags&0x4000)&&cache.parts.size()==1){
        const auto& p=packet->parts[0];const auto& drawn=cache.parts[0];
        if(drawn.art&&drawn.source.image==p.image){
            next.art=drawn.art;next.image=p.image;next.flags=p.flags&0xc080;next.attr=drawn.attr;next.base=drawn.base;
            next.flip=(next.attr&0x0800)!=0;next.shape=pose_shape(*drawn.art,next.flip);known=next.shape.valid&&!(next.attr&0x1000);
        }
    }
    bool continuous=known&&m.known&&tick>m.committed&&tick-m.committed<=12&&
        cache.focal>0&&std::abs(cache.focal-2.5*view_focal)<1e-9;
    if(pose_following){commit_follow(m,next,known,continuous,pose_follow_rival);return;}   // prototype 03: same qualification as 02
    if(!continuous)m.active=false;
    else if(next.image!=m.to.image||next.flags!=m.to.flags){
        PoseShape shown=m.to.shape;PoseTexture texture=m.to;
        double u=m.active?pose_progress(m,tick-1):1.;
        if(u<1){shown=pose_between(m.start,m.to.shape,pose_curve(u));if(u<.5)texture=m.from;}
        double reach=std::max(pose_distance(shown,next.shape),std::abs(shown.height-next.shape.height));
        // Beyond this it is another kind of image (crash, attack reach), not
        // a lean or posture step: keep the native swap.
        if(reach>.05&&reach<=20.){
            m.from=texture;m.start=shown;m.begin=tick;m.duration=pose_morph_frames;m.active=true;m.built=UINT64_MAX;++pose_eases;
        }else{m.active=false;if(reach>20.)++pose_ease_rejections;}
    }
    if(known)m.to=next;
    m.known=known;m.committed=tick;
}
// Rebuilds the in-between image for this frame. False = show the native pose.
bool build_morph(ActorCache& cache){
    auto& m=cache.morph;double u=pose_progress(m,tick);
    if(tick<m.begin||u>=1){m.active=false;return false;}
    if(m.built==tick)return true;
    double s=pose_curve(u);
    // Before the midpoint the previous image is deformed; after it, the new
    // one. Both are then half-way, so their silhouettes agree at the switch.
    const PoseTexture& t=u<.5?m.from:m.to;const auto& art=*t.art;
    double height=m.start.height+(m.to.shape.height-m.start.height)*s,source_height=t.shape.height;
    int rows=(int)std::ceil(height-1e-9);
    struct Span {int y;double l,r,sl,sr;};
    std::vector<Span> spans((size_t)std::max(rows,0));double lo=1e9,hi=-1e9;
    for(int j=0;j<rows;++j){auto& row=spans[j];row.y=-1;
        double hn=(rows-j-.5)/height;if(hn>=1)continue;
        int y=(int)std::floor(art.anchor_y-hn*source_height),l=-1,r=-1;
        if(y<0||y>=art.height)continue;
        for(int x=0;x<art.width;++x)if(art.pixels[(size_t)y*art.width+x]){if(l<0)l=x;r=x+1;}
        if(l<0)continue;
        double sl=l-art.anchor_x,sr=r-art.anchor_x;if(t.flip){double q=-sr;sr=-sl;sl=q;}
        int k=std::min(POSE_ROWS-1,(int)(hn*POSE_ROWS));
        double tl=t.shape.left[k],tr=t.shape.right[k];if(tr<=tl){tl=sl;tr=sr;}
        auto edge=[&](const PoseShape& p,double& a,double& b){if(p.right[k]<=p.left[k]){a=tl;b=tr;}else{a=p.left[k];b=p.right[k];}};
        double al,ar,bl,br;edge(m.start,al,ar);edge(m.to.shape,bl,br);
        // Displace this row's own exact hull by the interpolated difference.
        row.sl=sl;row.sr=sr;row.l=sl+(al+(bl-al)*s-tl);row.r=sr+(ar+(br-ar)*s-tr);
        if(row.r-row.l<1)continue;
        row.y=y;lo=std::min(lo,row.l);hi=std::max(hi,row.r);
    }
    if(hi<=lo){m.active=false;return false;}
    int left=(int)std::floor(lo),width=(int)std::ceil(hi)-left;
    auto& f=m.frame;f.width=width;f.height=rows;f.anchor_x=-left;f.anchor_y=rows;f.palette=art.palette;
    f.pixels.assign((size_t)width*rows,0);
    for(int j=0;j<rows;++j){const auto& row=spans[j];if(row.y<0)continue;
        for(int i=0;i<width;++i){double xa=left+i+.5;if(xa<row.l||xa>=row.r)continue;
            double us=row.sl+(xa-row.l)*(row.sr-row.sl)/(row.r-row.l);
            int x=(int)std::floor(t.flip?art.anchor_x-us:art.anchor_x+us);if(x<0||x>=art.width)continue;
            f.pixels[(size_t)j*width+i]=art.pixels[(size_t)row.y*art.width+x];
        }
    }
    f.coverage.build(width,rows,f.pixels.data());
    m.part=ActorArt{&f,SourcePart{cache.root.x,cache.root.y,cache.root.scale,152,t.image,0,t.flags,0},t.attr&~0x1800u,t.base,false};
    m.built=tick;return true;
}
// ---- Prototype 04: temporal pixel dissolve (RR_MOTORCYCLE_MOTION=6) ----------
// Only the riding lean ladders (ids 0-6 and 16-22 of the RR1 image table);
// combat, falls, start/recovery cycles and mirror changes keep the native swap.
bool dissolve_riding_pose(unsigned id){return id<=6||(id>=16&&id<=22);}
DissolveImage dissolve_image(const rr::SpriteAsset& a,bool flip){
    DissolveImage out;out.width=a.width;out.height=a.height;out.anchor_y=a.anchor_y;
    out.anchor_x=flip?a.width-a.anchor_x:a.anchor_x;out.pixels=a.pixels;
    if(flip)for(int y=0;y<a.height;++y)
        std::reverse(out.pixels.begin()+(size_t)y*a.width,out.pixels.begin()+(size_t)(y+1)*a.width);
    return out;
}
bool dissolve_stats(const DissolveImage& i,double& cx,double& top,double& bottom){
    double sum=0;size_t n=0;top=1e9;bottom=-1e9;
    for(int y=0;y<i.height;++y)for(int x=0;x<i.width;++x)if(i.pixels[(size_t)y*i.width+x]){
        sum+=x-i.anchor_x;++n;top=std::min(top,(double)(y-i.anchor_y));bottom=std::max(bottom,(double)(y-i.anchor_y+1));}
    if(!n)return false;cx=sum/n;return true;
}
// A pose swap, not another kind of image: similar horizontal mass and extent.
bool dissolve_compatible(const DissolveImage& a,const DissolveImage& b){
    double ax,at,ab,bx,bt,bb;
    if(!dissolve_stats(a,ax,at,ab)||!dissolve_stats(b,bx,bt,bb))return false;
    return std::abs(ax-bx)<=24&&std::abs(at-bt)<=24&&std::abs(ab-bb)<=10;
}
unsigned dissolve_pixel(const DissolveImage& i,int rx,int ry){
    int x=i.anchor_x+rx,y=i.anchor_y+ry;
    return x<0||y<0||x>=i.width||y>=i.height?0u:i.pixels[(size_t)y*i.width+x];
}
// Pixels with Bayer rank < threshold (of 16) show the new image.
DissolveImage dissolve_blend(const DissolveImage& from,const DissolveImage& to,unsigned threshold){
    static constexpr unsigned bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    int left=-std::max(from.anchor_x,to.anchor_x),right=std::max(from.width-from.anchor_x,to.width-to.anchor_x);
    int top=-std::max(from.anchor_y,to.anchor_y),bottom=std::max(from.height-from.anchor_y,to.height-to.anchor_y);
    DissolveImage out;out.width=right-left;out.height=bottom-top;out.anchor_x=-left;out.anchor_y=-top;
    out.pixels.assign((size_t)out.width*out.height,0);
    for(int oy=0;oy<out.height;++oy)for(int ox=0;ox<out.width;++ox){
        int rx=left+ox,ry=top+oy;
        out.pixels[(size_t)oy*out.width+ox]=(uint8_t)(bayer[ry&3][rx&3]<threshold?dissolve_pixel(to,rx,ry):dissolve_pixel(from,rx,ry));
    }
    return out;
}
// Called when the native image of an actor is (re)committed. `known` = a
// complete, single-priority, unflipped-vertically rider image.
int dissolve_commit(Dissolve& d,DissolveImage&& image,unsigned id,unsigned second,unsigned flags,
                            unsigned attr,unsigned base,bool known){
    if(!known){d.active=false;d.known=false;return -1;}
    bool same=d.known&&d.id==id&&d.second==second&&d.flags==flags;
    if(same)return 0;int reason=7;
    unsigned interval=d.known&&tick>d.committed?(unsigned)(tick-d.committed):0;
    bool candidate=d.known&&interval>0&&interval<=12&&(d.attr&0xe800)==(attr&0xe800)&&d.base==base&&
        dissolve_riding_pose(id)&&dissolve_riding_pose(d.id)&&(d.flags&0x8000)==(flags&0x8000);
    bool mixing=d.active&&d.built!=UINT64_MAX&&d.shown.width>0;
    d.active=false;if(d.known&&!candidate)reason=5;
    if(candidate){
        // An interrupted transition continues from what is on screen: the new
        // pose cancels the old one without a snap.
        const DissolveImage& source=mixing?d.shown:d.to;
        if(dissolve_compatible(source,image)){
            DissolveImage keep=source;d.from=std::move(keep);d.from_id=d.id;
            d.frames=(unsigned)std::clamp((int)interval-1,1,3);d.begin=tick;d.active=true;d.built=UINT64_MAX;
            d.shown=DissolveImage{};++dissolve_swaps;reason=0;
        }else reason=6;
    }
    d.to=std::move(image);d.id=id;d.second=second;d.flags=flags;d.attr=attr;d.base=base;
    d.known=true;d.committed=tick;return reason;
}
// Builds this frame's mix. False = show the native pose (transition over).
bool build_dissolve(Dissolve& d,const Pose& root,uint32_t actor,bool player){
    if(!d.active)return false;
    if(tick<d.begin||tick-d.begin>=d.frames){d.active=false;return false;}
    if(d.built==tick)return true;
    unsigned k=(unsigned)(tick-d.begin)+1,threshold=(16*k+(d.frames+1)/2)/(d.frames+1);
    d.shown=dissolve_blend(d.from,d.to,threshold);
    auto& f=d.frame;f.width=d.shown.width;f.height=d.shown.height;f.anchor_x=d.shown.anchor_x;f.anchor_y=d.shown.anchor_y;
    f.palette=0;f.pixels=d.shown.pixels;f.coverage.build(f.width,f.height,f.pixels.data());
    d.part=ActorArt{&f,SourcePart{root.x,root.y,root.scale,152,d.id,d.second,d.flags,0},d.attr&~0x1800u,d.base,false};
    d.built=tick;++dissolve_frames;
    if(dissolve_trace)std::fprintf(dissolve_trace,"%llu,%s,%06X,%u,%u,%u,%u,%u,%.3f\n",(unsigned long long)tick,player?"player":"rival",
        actor,d.from_id,d.id,k,d.frames,threshold,(double)threshold/16.);
    return true;
}
void commit_dissolve(ActorCache& cache,const SourcePacket *packet){
    DissolveImage image;unsigned id=0,flags=0,attr=0,base=0;bool known=false;int why=1;
    // Same qualification as the other prototypes: ONE complete rider image that
    // classic already draws from its own qualified source (rivals have no overlay).
    if(packet&&packet->parts.size()==1&&!packet->parts[0].second&&!(packet->parts[0].flags&0x4000)&&cache.parts.size()==1){
        const auto& p=packet->parts[0];const auto& drawn=cache.parts[0];
        why=!(drawn.art&&drawn.source.image==p.image)?2:(drawn.attr&0x1000)?3:!(cache.focal>0&&std::abs(cache.focal-2.5*view_focal)<1e-9)?4:0;id=p.image;
        if(!why){
            image=dissolve_image(*drawn.art,(drawn.attr&0x0800)!=0);id=p.image;flags=p.flags&0xc080;
            attr=drawn.attr;base=drawn.base;known=image.width>0;
        }
    }
    const Dissolve before=cache.dissolve;
    int result=dissolve_commit(cache.dissolve,std::move(image),id,0,flags,attr,base,known);
    if(dissolve_trace&&result!=0){int reason=result<0?why:result;   // diagnostic: why a swap stayed native
        std::fprintf(dissolve_trace,"%llu,rival-skip,%06X,%u,%u,%d,0,0,0,attr %04X>%04X base %X>%X flags %04X>%04X age %llu\n",(unsigned long long)tick,cache.actor,before.id,id,reason,
            before.attr,attr,before.base,base,before.flags,flags,(unsigned long long)(tick-before.committed));}
}
// The player's complete native bitmap (bike, rider and the `second` overlay with
// their own palette lines) is the unit: each layer thus dissolves from its own
// old pixels to its own new pixels. Native SAT stays authoritative for position.
bool track_player_dissolve(const GVDP *v,const Job& job,double x){
    auto& cache=player_cache;auto& d=cache.dissolve;
    cache.root=Pose{x,job.y,1.};
    if(job.serial!=player_serial){
        player_serial=job.serial;
        DissolveImage image;bool known=false;unsigned high=2;
        ActorCanvas canvas;const std::vector<const Job*> one{&job};
        if(!(job.flags&0x4000)&&compose_actor(v,one,cache.root,canvas)&&canvas.width>0&&canvas.height>0){
            bool uniform=true;
            for(size_t n=0;n<canvas.opaque.size();++n)if(canvas.opaque[n]){
                if(high==2)high=canvas.high[n];else uniform&=canvas.high[n]==high;}
            if(uniform&&high<2){
                image.width=canvas.width;image.height=canvas.height;
                image.anchor_x=(int)std::lround(x-canvas.x);image.anchor_y=(int)std::lround(job.y-canvas.y);
                image.pixels.assign((size_t)canvas.width*canvas.height,0);
                for(size_t n=0;n<canvas.opaque.size();++n)if(canvas.opaque[n])image.pixels[n]=canvas.index[n];
                known=true;
            }
        }
        dissolve_commit(d,std::move(image),job.image,job.second,job.flags&0xc080,high==1?0x8000u:0u,0,known);
    }
    return build_dissolve(d,cache.root,(uint32_t)view.player,true);
}
void commit_pose(ActorCache& cache,const SourcePacket *packet){
    if(pose_dissolving)commit_dissolve(cache,packet);
    if(pose_morphing||pose_following)commit_morph(cache,packet);else cache.morph.active=cache.morph.known=false;
    const PoseEase previous=cache.ease;
    PoseEase next;next.committed=tick;
    if(!pose_easing){cache.ease=next;return;}
    // Only one complete, unflipped-vertically rider image is a known pose.
    // Crash pieces, overlays and cars keep the committed native presentation.
    if(packet&&packet->parts.size()==1&&!packet->parts[0].second&&!(packet->parts[0].flags&0x4000)){
        const auto& p=packet->parts[0];
        if(const auto *art=asset(p.image,p.flags)){
            next.line=pose_line(*art,(p.flags&0x8000)!=0);next.image=p.image;next.flags=p.flags&0xc080;next.known=next.line.valid;
        }
    }
    if(next.known&&previous.known&&tick>previous.committed&&tick-previous.committed<=12&&
       cache.focal>0&&std::abs(cache.focal-2.5*view_focal)<1e-9){
        // What the previous frame displayed: its image line plus the share of
        // an unfinished residual. Chained lean steps therefore stay continuous.
        double carried=pose_ease_factor(previous,tick-1);
        double offset=previous.line.offset+previous.offset*carried-next.line.offset;
        double slope=previous.line.slope+previous.slope*carried-next.line.slope;
        if(next.image==previous.image&&next.flags==previous.flags){
            // The same image recommitted by a later native update: continue.
            next.active=previous.active;next.start=previous.start;next.duration=previous.duration;
            next.offset=previous.offset;next.slope=previous.slope;
        }else{
            double height=std::max(previous.line.height,next.line.height);
            double reach=std::max(std::abs(offset),std::abs(offset+slope*height));
            // A larger difference is a different kind of image, not a lean step.
            if(reach>.05&&reach<=10.){
                next.active=true;next.start=tick;next.offset=offset;next.slope=slope;
                next.duration=std::clamp((double)(tick-previous.committed),3.,8.);++pose_eases;
            }else if(reach>10.)++pose_ease_rejections;
        }
    }
    cache.ease=next;
}
void source_motion(ActorCache& cache,const SourcePacket& packet){
    if(packet.parts.empty()||rear_view(packet)||packet.epoch<=cache.motion.serial)return;
    const auto& p=packet.parts.front();Pose next{p.x,p.y,p.scale};auto& m=cache.motion;
    if(!m.samples.empty()){
        const auto& last=m.samples.back();
        if(packet.tick<=last.first||packet.tick-last.first>18||std::abs(next.x-last.second.x)>256||
           std::abs(next.y-last.second.y)>160||next.scale/last.second.scale<.25||next.scale/last.second.scale>4){
            m.samples.clear();cache.depth_filter.reset();cache.body_filter.reset();cache.height_filter.reset();
        }
    }
    m.samples.emplace_back(packet.tick,next);while(m.samples.size()>12)m.samples.pop_front();
    m.current=next;m.serial=packet.epoch;m.seen=packet.tick;m.art.projection.actor=1;
    cache.appearance=packet;
}

Pose vehicle_sample(ActorCache& cache,const std::deque<SourcePacket>& packets,double& clip,double& display_depth,bool& hill_anchor){
    const auto& p=packets.back();auto appearance=packets.rbegin();
    cache.lane_anchor=cache.raw_height=false;
    while(appearance!=packets.rend()&&(appearance->parts.empty()||rear_view(*appearance)))++appearance;
    const auto& art_packet=appearance==packets.rend()||cache.appearance.epoch>appearance->epoch?cache.appearance:*appearance;
    if(art_packet.parts.empty())return cache.motion.current;
    const auto& b=art_packet.parts.front();
    clip=std::min((double)view_bottom,b.clip);display_depth=0;hill_anchor=false;
    double art_depth=2.+art_packet.world-art_packet.camera;
    double native_depth=2.+p.world-p.camera,focal=b.scale*art_depth;
    // The verified native vehicle projection uses f=2.5 (riders) or 3 (cars).
    // Recognize its real depth, rather than predicting height about y=64.
    if(art_packet.camera>0&&art_depth>=2.){
        if(std::abs(focal-2.5*view_focal)<.04)cache.focal=2.5*view_focal;
        else if(std::abs(focal-3.*view_focal)<.04)cache.focal=3.*view_focal;
        else cache.focal=0;
    }
    // A crash can reuse one actor for separately placed rider/bike images,
    // or change its projection convention. An old riding calibration must
    // never turn these into one rigid, enlarged vehicle.
    if(!rigid_vehicle(art_packet))cache.focal=0;
    if(!cache.focal){cache.world_height_valid=cache.departing=false;cache.lane_frame=UINT64_MAX;
        return art_packet.camera>0?Pose{b.x,b.y,b.scale}:scene_sample(cache.motion);}
    double elapsed=0,relative_velocity=0,lateral=p.lateral,lateral_velocity=0,height_velocity=0;
    if(packets.size()>1){const auto& a=packets[packets.size()-2];
        double interval=(double)(p.tick-a.tick);
        if(interval>0&&interval<=12){
            elapsed=std::min((double)(tick-p.tick),interval);
            relative_velocity=((p.world-p.camera)-(a.world-a.camera))/interval;
            // Reused actors/teleports are observations, not approach velocity.
            if(std::abs(relative_velocity)>.5)relative_velocity=0;
            lateral_velocity=(p.lateral-a.lateral)/interval;
            height_velocity=(p.height-a.height)/interval;
            // A single last difference overshoots a steering reversal, then
            // snaps back at the next native update. Only forecast lateral
            // motion supported by both observed intervals.
            if(packets.size()>2){const auto& prior=packets[packets.size()-3];
                double previous_interval=(double)(a.tick-prior.tick);
                if(previous_interval>0&&previous_interval<=12){
                    double previous=(a.lateral-prior.lateral)/previous_interval;
                    if(previous*lateral_velocity<=0)lateral_velocity=0;
                    else lateral_velocity=std::copysign(std::min(std::abs(previous),std::abs(lateral_velocity)),lateral_velocity);
                }
            }
        }
    }
    // Both relative-depth endpoints must come from the same native actor
    // update. Subtracting an independently timed camera forecast produced a
    // five-frame sawtooth even when the rider maintained a constant gap.
    double projected_depth=native_depth+relative_velocity*elapsed;
    bool continuous_height=art_packet.road.has_height&&p.road.has_height;
    if(cache.depth_filter.frame!=UINT64_MAX&&std::abs(projected_depth-cache.depth_filter.value)>std::max(.5,std::abs(projected_depth)*.4)){
        cache.depth_filter.reset();cache.body_filter.reset();cache.height_filter.reset();
    }
    double depth_response=.35+.25*std::clamp((2.5-projected_depth)/.5,0.,1.);
    display_depth=std::max(.25,continuous_height?cache.depth_filter.track(tick,projected_depth,relative_velocity,.25,depth_response):projected_depth);
    bool rear=rear_view(p);
    if(rear&&cache.departing)display_depth=std::min(display_depth,cache.departure_depth);
    if(!rear)cache.departing=false;
    double projection_scale=cache.focal/display_depth;
    // Depth 2.5 is a calibration plane, not a maximum artwork size. Keep
    // position and size on the SAME perspective after an actor passes that
    // plane, so it grows into the camera rather than sliding down at 1x.
    const auto& local=p.has_road?p.road:art_packet.road;
    const auto current=presentation_road(roads.empty()||std::abs(roads.back().rays.camera-p.camera)>4?local:roads.back().rays,vehicle_camera_at(p.camera));
    double table_depth=display_depth+vehicle_camera_at(p.camera)-current.camera;
    double road_x=0,art_x=0;
    bool lane=rr::road_lateral(current,table_depth,road_x)&&rr::road_lateral(art_packet.road,art_depth,art_x);
    double art_position=rr::road_position_depth(art_packet.road,art_depth);
    double position=rr::road_position_depth(current,display_depth),table_position=rr::road_position_depth(current,table_depth);
    double anchor_x=lane?b.x-(view_center+(art_x+art_packet.lateral)/art_position):0;
    // The native grid/close-combat path has real anchor corrections, not
    // merely <=8px rounding. Keep the measured anchor in world units and
    // project it with the same depth as the rest of this complete pose.
    // Discarding it moved a stationary grid rider when the model took over.
    if(lane&&std::abs(anchor_x)>128){
        cache.focal=0;cache.world_height_valid=cache.departing=false;cache.lane_frame=UINT64_MAX;
        return {b.x,b.y,b.scale};
    }
    anchor_x=cache.body_filter.step(tick,anchor_x*art_position,.5*art_position)/position;
    cache.lane_anchor=lane;cache.curve_center=view_center+road_x/position;
    cache.lane_target=lateral+lateral_velocity*elapsed;
    cache.lane_world=smooth_vehicle_lane(cache,cache.lane_target,lateral_velocity,position);cache.body_offset=anchor_x;
    Pose pose{lane?view_center+(road_x+cache.lane_world)/position+anchor_x:
                   view_center+(b.x-view_center)/b.scale*projection_scale,b.y,
              projection_scale};
    double anchor=0,unused=(double)view_bottom;
    if(art_packet.has_road&&b.y<=view_bottom+8&&rr::road_point(art_packet.road,art_depth,anchor,unused)&&std::abs(b.y-anchor)<=8){
        double ground=0;
        if(rr::road_point(current,table_depth,ground,clip)){pose.y=current.horizon+(ground-current.horizon)*table_position/position+(b.y-anchor)*art_position/position;hill_anchor=true;}
    }
    double raw_anchor=0,raw_y=0;
    if(!rear_view(art_packet)&&b.y<view_bottom&&art_packet.road.has_height&&
       rr::road_vehicle_height(art_packet.road,art_depth,art_packet.height,raw_anchor)){
        cache.world_height_valid=std::abs(b.y-raw_anchor)<=64;
        if(cache.world_height_valid)cache.height_bias=(b.y-raw_anchor)*art_position;
    }
    if(cache.world_height_valid&&current.has_height&&rr::road_vehicle_height(current,table_depth,p.height+height_velocity*elapsed,raw_y)){
        pose.y=current.horizon+(raw_y-current.horizon)*table_position/position+
            cache.height_filter.step(tick,cache.height_bias,.5*art_position)/position;cache.raw_height=true;
        double unused_y=0;if(rr::road_point(current,table_depth,unused_y,clip))hill_anchor=true;
    }
    // A qualified unclipped ground anchor is required for the near extension.
    // Unknown profiles retain the conservative player-plane limit.
    if(!cache.raw_height)pose.scale=std::min(pose.scale,cache.focal/2.5);
    if(rear){
        double extent=std::max(0.,(cache.root.y-cache.canvas.y)*pose.scale/cache.root.scale);
        double top=pose.y-extent;
        if(cache.departing){
            double dt=tick>=cache.departure_frame?std::min(12.,(double)(tick-cache.departure_frame)):0;
            if(dt>0&&top>cache.departure_top)cache.departure_speed=std::clamp((top-cache.departure_top)/dt,2.,12.);
            double remaining=std::max(1.,32.-(double)(tick-art_packet.tick));
            cache.departure_speed=std::max(cache.departure_speed,std::clamp((view_bottom+1.-cache.departure_top)/remaining,2.,12.));
            // Advancing only the feet lets a growing roof/helmet re-enter
            // above the HUD during braking or a grade change. Complete the
            // one-way pass with the whole forward canvas, not its foot alone.
            top=std::max(top,cache.departure_top+cache.departure_speed*dt);
            pose.y=top+extent;
        }else cache.departure_speed=2;
        cache.departing=true;cache.departure_depth=display_depth;cache.departure_top=top;
        cache.departure_frame=tick;
    }
    return pose;
}

bool compose_actor(const GVDP *v,const std::vector<const Job*>& jobs,const Pose& root,ActorCanvas& out,
                   ActorCache *cache=nullptr,const SourcePacket *packet=nullptr,std::vector<ActorArt> *full_art=nullptr){
    struct Cell {unsigned slot;const Piece *piece;int x,y,w,h;};std::vector<Cell> cells;
    int left=9999,top=9999,right=-9999,bottom=-9999;
    for(const Job *job:jobs)for(unsigned n=0;n<job->pieces.size();++n){
        const auto& p=job->pieces[n];int x=unwrap_x(((p.sat[6]<<8|p.sat[7])&511)-128,root.x);
        int y=((p.sat[0]<<8|p.sat[1])&511)-128;
        if(y>root.y+128)y-=512;
        int w=(((p.sat[2]>>2)&3)+1)*8,h=((p.sat[2]&3)+1)*8;
        cells.push_back({(unsigned)job->first+n,&p,x,y,w,h});
        left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+w);bottom=std::max(bottom,y+h);
    }
    std::vector<ActorArt> parts;
    if(cache)for(const Job *job:jobs)if(!job->pieces.empty()){
        const auto& p=job->pieces.front();unsigned attr=(p.sat[4]<<8)|p.sat[5],pal=(attr>>13)&3;
        int ext=pal==0?gvdp_sprite_pattern_line(p.sat[2],attr&2047):0;
        unsigned base=ext?GVDP_HOST_PALETTE_BASE+(ext-1)*16:pal*16;
        cache->palettes[palette_key(job->image,job->second,job->flags)]=attr|(base<<16);
    }
    if(cache&&packet&&edge_recovery())for(const auto& source:packet->parts){
        if(source.scale<=0||source.scale>8)continue;
        bool native=false;
        for(const Job *job:jobs){double x=job->x+(game_index==0&&job->x<0?512.:0.);
            native|=job->image==source.image&&job->second==source.second&&
                (job->flags&0xc080)==(source.flags&0xc080)&&std::abs(x-source.x)<=4&&std::abs(job->y-source.y)<=4;}
        auto add=[&](unsigned image){
            const auto *art=asset(image,source.flags);if(!art)return;
            unsigned learned=(art->palette<<13)|0x8000|((source.flags&0x8000)?0x0800:0)|(art->palette*16<<16);
            auto found=cache->palettes.find(palette_key(source.image,source.second,source.flags));
            if(found!=cache->palettes.end())learned=found->second;
            unsigned attr=learned&65535;int ax=(attr&0x0800)?art->width-art->anchor_x:art->anchor_x;
            int ay=(attr&0x1000)?art->height-art->anchor_y:art->anchor_y;
            double x=source.x-ax*source.scale,y=source.y-ay*source.scale;
            left=std::min(left,std::max(-128,(int)std::floor(x)));top=std::min(top,std::max(-128,(int)std::floor(y)));
            right=std::max(right,std::min(448,(int)std::ceil(x+art->width*source.scale)));
            bottom=std::max(bottom,std::min(384,(int)std::ceil(y+art->height*source.scale)));
            parts.push_back({art,source,attr,learned>>16,native});
        };
        add(source.image);if(source.second)add(source.second);
    }
    if(!parts.empty()){left=std::max(left,-128);right=std::min(right,448);top=std::max(top,-128);bottom=std::min(bottom,384);}
    int limit=parts.empty()?384:1024;
    if((cells.empty()&&parts.empty())||right<=left||bottom<=top||right-left>limit||bottom-top>limit)return false;
    ActorCanvas canvas;canvas.x=left;canvas.y=top;canvas.width=right-left;canvas.height=bottom-top;
    size_t count=(size_t)canvas.width*canvas.height;
    canvas.index.resize(count);canvas.opaque.resize(count);canvas.high.resize(count);
    std::sort(cells.begin(),cells.end(),[](const Cell&a,const Cell&b){return a.slot<b.slot;});
    for(const auto& cell:cells){
        const auto& p=*cell.piece;unsigned attr=(p.sat[4]<<8)|p.sat[5],pal=(attr>>13)&3;
        unsigned bytes=0;const uint8_t *patterns=gvdp_sprite_patterns(p.sat[2],&bytes);
        for(int y=0;y<cell.h;++y)for(int x=0;x<cell.w;++x){
            if(cell.x+x<left||cell.x+x>=right||cell.y+y<top||cell.y+y>=bottom)continue;
            int u=(attr&0x0800)?cell.w-1-x:x,w=(attr&0x1000)?cell.h-1-y:y;
            unsigned tile=((attr&2047)+(u>>3)*(cell.h/8)+(w>>3))&2047;
            unsigned at=tile*32+(w&7)*4+(u&7)/2;
            unsigned byte=patterns?(at<bytes?patterns[at]:0):v->vram[at&65535];
            unsigned nib=(byte>>((u&1)?0:4))&15;if(!nib)continue;
            // Operator sprites must stay in the VDP path, rather than being
            // mistaken for visible palette colors in an assembly snapshot.
            if((v->reg[12]&8)&&pal==3&&nib>=14)return false;
            size_t dest=(size_t)(cell.y-top+y)*canvas.width+(cell.x-left+x);
            if(canvas.opaque[dest])continue;
            int ext=pal==0?gvdp_sprite_pattern_line(p.sat[2],tile):0;
            canvas.index[dest]=(uint8_t)(ext?GVDP_HOST_PALETTE_BASE+(ext-1)*16+nib:pal*16+nib);
            canvas.opaque[dest]=1;canvas.high[dest]=(uint8_t)(attr>>15);
        }
    }
    // Native auxiliary/body order can be the reverse of descriptor order.
    // Apply it to qualification AND to recovered margins, not only drawing.
    if(!jobs.empty())std::stable_sort(parts.begin(),parts.end(),[&](const ActorArt& a,const ActorArt& b){
        auto order=[&](const ActorArt& p){unsigned first=80;
            for(const Job *job:jobs)if(job->address==p.source.address)first=std::min(first,(unsigned)job->first);
            return first;};
        return order(a)<order(b);
    });
    if(full_art){
        full_art->clear();
        // Direct source sampling avoids scaling an already quantized native
        // bitmap a second time. Authorize the whole assembly only when its
        // current source colors/silhouette agree with the committed pose.
        // Unsupported recolors/poses keep the complete native canvas.
        bool complete=!parts.empty()&&std::all_of(parts.begin(),parts.end(),[&](const ActorArt& p){return jobs.empty()||p.native;});
        unsigned compared=0,different=0;
        bool footprint_test=coverage_enabled&&parts.size()==1;
        std::vector<std::array<uint64_t,4>> footprint(footprint_test?count:0);
        std::vector<uint8_t> candidate(count),candidate_high(count);
        if(complete&&!jobs.empty())for(int y=std::max(view_top,top);y<std::min(view_bottom,bottom);++y)
            for(int x=std::max(view_left,left);x<std::min(view_right,right);++x){
                unsigned color=0,high=0;
                for(const auto& p:parts){const auto& s=p.source;const auto& art=*p.art;
                    int ax=(p.attr&0x0800)?art.width-art.anchor_x:art.anchor_x;
                    int ay=(p.attr&0x1000)?art.height-art.anchor_y:art.anchor_y;
                    int u=(int)std::floor((x+.5-s.x)/s.scale+ax),w=(int)std::floor((y+.5-s.y)/s.scale+ay);
                    if(u<0||u>=art.width||w<0||w>=art.height||y+.5>=s.clip)continue;
                    if(p.attr&0x0800)u=art.width-1-u;if(p.attr&0x1000)w=art.height-1-w;
                    unsigned nib=art.pixels[w*art.width+u];if(!nib)continue;
                    color=p.base+nib;high=(p.attr>>15)&1;break;
                }
                size_t at=(size_t)(y-top)*canvas.width+x-left;
                candidate[at]=(uint8_t)color;candidate_high[at]=(uint8_t)high;
                if(footprint_test){const auto& p=parts.front();const auto& s=p.source;const auto& art=*p.art;
                    int ax=(p.attr&0x0800)?art.width-art.anchor_x:art.anchor_x;
                    int ay=(p.attr&0x1000)?art.height-art.anchor_y:art.anchor_y;
                    int u0=std::max(0,(int)std::floor((x-s.x)/s.scale+ax));
                    int u1=std::min(art.width,(int)std::ceil((x+1.-s.x)/s.scale+ax));
                    int w0=std::max(0,(int)std::floor((y-s.y)/s.scale+ay));
                    int w1=std::min(art.height,(int)std::ceil((std::min(y+1.,s.clip)-s.y)/s.scale+ay));
                    for(int w=w0;w<w1;++w)for(int u=u0;u<u1;++u){
                        int fx=(p.attr&0x0800)?art.width-1-u:u,fy=(p.attr&0x1000)?art.height-1-w:w;
                        unsigned nib=art.pixels[fy*art.width+fx];if(!nib)continue;
                        unsigned c=(p.base+nib)&255;footprint[at][c/64]|=uint64_t(1)<<(c%64);
                    }
                }
            }
        // The packed-byte scaler rounds horizontal pairs and row origins
        // independently. A non-unit reduction has up to two native pixels of
        // phase, not one. Keep the SAME bidirectional color/priority test and
        // 2% limit; do not confuse that quantization with an unsupported pose.
        int phase=coverage_enabled&&std::abs(root.scale-1.)>1./16384.?2:1;
        auto supported=[&](int x,int y,unsigned color,unsigned high,bool native){
            for(int dy=-phase;dy<=phase;++dy)for(int dx=-phase;dx<=phase;++dx){int nx=x+dx,ny=y+dy;
                if(nx<left||nx>=right||ny<top||ny>=bottom||nx<view_left||nx>=view_right||ny<view_top||ny>=view_bottom)continue;
                size_t at=(size_t)(ny-top)*canvas.width+nx-left;
                if(footprint_test){
                    // Native byte/row sampling may discard a source color. Test
                    // source silhouette against native coverage, and EVERY
                    // committed native color against the full current-source
                    // footprint, rather than another point-sampled minifier.
                    if(native?canvas.opaque[at]&&canvas.high[at]==high:
                       ((footprint[at][color/64]>>(color%64))&1)&&((parts.front().attr>>15)&1)==high)return true;
                }else if(native?canvas.opaque[at]&&canvas.index[at]==color&&canvas.high[at]==high:
                    candidate[at]==color&&candidate_high[at]==high)return true;
            }
            return false;
        };
        if(complete&&!jobs.empty())for(int y=std::max(view_top,top);y<std::min(view_bottom,bottom);++y)
            for(int x=std::max(view_left,left);x<std::min(view_right,right);++x){
                size_t at=(size_t)(y-top)*canvas.width+x-left;
                if(candidate[at]){++compared;different+=!supported(x,y,candidate[at],candidate_high[at],true);}
                if(canvas.opaque[at]){++compared;different+=!supported(x,y,canvas.index[at],canvas.high[at],false);}
            }
        bool accepted=complete&&(jobs.empty()||(compared>=16&&different*100<=compared*2));
        if(source_audit)std::fprintf(source_audit,"%llu,%06X,%llu,%zu,%zu,%u,%u,%u,%d,%.7f,%.4f,%.4f\n",
            (unsigned long long)tick,cache?cache->actor:0,(unsigned long long)(packet?packet->epoch:0),parts.size(),jobs.size(),
            (unsigned)std::count_if(parts.begin(),parts.end(),[](const ActorArt& p){return p.native;}),compared,different,accepted?1:0,root.scale,root.x,root.y);
        if(accepted)*full_art=parts;
    }
    // Preserve the committed animation in the original viewport. Recover only
    // data removed by its software clip, before applying the presentation zoom.
    for(const auto& p:parts){
        const auto& s=p.source;const auto& art=*p.art;bool hf=p.attr&0x0800,vf=p.attr&0x1000;
        int ax=hf?art.width-art.anchor_x:art.anchor_x,ay=vf?art.height-art.anchor_y:art.anchor_y;
        double x0=s.x-ax*s.scale,y0=s.y-ay*s.scale;
        int xa=std::max(left,(int)std::ceil(x0-.5)),xb=std::min(right,(int)std::ceil(x0+art.width*s.scale-.5));
        int ya=std::max(top,(int)std::ceil(y0-.5)),yb=std::min(bottom,(int)std::ceil(std::min(y0+art.height*s.scale,s.clip)-.5));
        // A clip at the HUD is a viewport boundary, not a hill occluder. The
        // final destination is still clipped to the race area after scaling.
        if(s.clip>=view_bottom)yb=std::min(bottom,(int)std::ceil(y0+art.height*s.scale-.5));
        for(int y=ya;y<yb;++y)for(int x=xa;x<xb;++x){
            if(p.native&&x>=view_left&&x<view_right&&y>=view_top&&y<view_bottom)continue;
            size_t at=(size_t)(y-top)*canvas.width+x-left;if(canvas.opaque[at])continue;
            int u=(int)std::floor((x+.5-x0)/s.scale),w=(int)std::floor((y+.5-y0)/s.scale);
            if(u<0||u>=art.width||w<0||w>=art.height)continue;if(hf)u=art.width-1-u;if(vf)w=art.height-1-w;
            unsigned nib=art.pixels[w*art.width+u];if(!nib)continue;
            if((v->reg[12]&8)&&p.base==48&&nib>=14)return false;
            canvas.index[at]=(uint8_t)(p.base+nib);canvas.opaque[at]=1;canvas.high[at]=(p.attr>>15)&1;++actor_restored_pixels;
        }
    }
    canvas.coverage.build(canvas.width,canvas.height,canvas.index.data(),canvas.opaque.data(),canvas.high.data());
    out=std::move(canvas);return true;
}

// RR1 rival name, read the way the game's own HUD routine ($00D73A) does it: slot+0x71 is the rider number n,
// the per-race table in RAM ($FF0546, or $FF0566 when $FF0508 is set) maps n to a record index, and the name is the
// first 10 bytes of the ROM record at $CCD0+(index+1)*16 (D038: the old formula was one record short).
// D040 RR2: the race's circuit (0..4) is the ROM group whose 15 records match the 15 rider slots ($FF0AE2+(n-1)*0x80: +0x74 = n,
// +0x17 = attribute b10, +0x77 = attribute b11). Groups differ in >=13 of 15 slots, so >=10 matches is unambiguous; fewer = menu/loading.
int rr2_group(){
    if(rr2_group_tick==tick)return rr2_group_value;
    rr2_group_tick=tick;rr2_group_value=-1;
    if(!source_ram||!source_rom||source_bytes<0x10b32u+75*16)return -1;
    unsigned best=0;
    for(unsigned g=0;g<5;++g){unsigned score=0;
        for(unsigned n=0;n<15;++n){unsigned slot=0x0ae2+n*0x80;const uint8_t *r=source_rom+0x10b32+(g*15+n)*16;
            if(source_ram[slot+0x74]==n+1&&r[0]==source_ram[slot+0x17]&&r[1]==source_ram[slot+0x77])++score;}
        if(score>best){best=score;rr2_group_value=(int)g;}}
    if(best<10)rr2_group_value=-1;
    return rr2_group_value;
}
// D044 RR3: slots $FF0A32+(n-1)*0x80 (+0x0E = n, +0x17 = attribute b10); records at $15F32, 5 circuits x 15.
int rr3_group(){
    if(rr3_group_tick==tick)return rr3_group_value;
    rr3_group_tick=tick;rr3_group_value=-1;
    if(!source_ram||!source_rom||source_bytes<0x15f32u+75*16)return -1;
    unsigned best=0;
    for(unsigned g=0;g<5;++g){unsigned score=0;
        for(unsigned n=0;n<15;++n){unsigned slot=0x0a32+n*0x80;const uint8_t *r=source_rom+0x15f32+(g*15+n)*16;
            if(source_ram[slot+0x0e]==n+1&&r[0]==source_ram[slot+0x17])++score;}
        if(score>best){best=score;rr3_group_value=(int)g;}}
    if(best<10)rr3_group_value=-1;
    return rr3_group_value;
}
bool rival_name(uint32_t actor,std::string& name){
    if(game_index==2){
        if(!source_ram||!source_rom)return false;
        unsigned a=actor&65535;
        if(a<0x0a32||a>0x0a32+14*0x80||(a-0x0a32)%0x80)return false;
        unsigned n=source_ram[(a+0x0e)&65535];if(n<1||n>15)return false;
        int g=rr3_group();if(g<0)return false;
        const uint8_t *r=source_rom+0x15f32+(g*15+n-1)*16;
        if(!r[3])return false;   // police record: never personalised
        name.assign((const char*)r+6,10);
        for(char c:name)if(c<32||c>126)return false;
        while(!name.empty()&&name.back()==' ')name.pop_back();
        return !name.empty();
    }
    if(game_index==1){
        if(!source_ram||!source_rom)return false;
        unsigned a=actor&65535;
        if(a<0x0ae2||a>0x0ae2+14*0x80||(a-0x0ae2)%0x80)return false;
        unsigned n=source_ram[(a+0x74)&65535];if(n<1||n>15)return false;
        int g=rr2_group();if(g<0)return false;
        const uint8_t *r=source_rom+0x10b32+(g*15+n-1)*16;   // record = attributes[6] + name[10]
        if(!r[3])return false;   // attribute b13 = 0 marks the police record (n=1 of every circuit): never personalised
        name.assign((const char*)r+6,10);
        for(char c:name)if(c<32||c>126)return false;
        while(!name.empty()&&name.back()==' ')name.pop_back();
        return !name.empty();
    }
    if(game_index!=0||!source_ram||!source_rom)return false;
    unsigned a=actor&65535;
    if(a<0x1f2a||a>0x262a||(a-0x1f2a)%0x80)return false;   // n=1..8 live in $1F2A..$22AA, n=9..15 in $232A..$262A
    auto word=[&](unsigned at){return (unsigned)((source_ram[at&65535]<<8)|source_ram[(at+1)&65535]);};
    unsigned n=source_ram[(a+0x71)&65535];
    if(n<1||n>15)return false;
    unsigned table=word(0x508)?0x566:0x546,index=word(table+n*2);
    if(index>=75)return false;
    unsigned at=0xccd0+(index+1)*16;if(at+10>source_bytes)return false;
    name.assign((const char*)source_rom+at,10);
    for(char c:name)if(c<32||c>126)return false;
    while(!name.empty()&&name.back()==' ')name.pop_back();
    return !name.empty();
}
// Helmet = the largest {1,2,3,4} blob high on the rider; vest = the largest {4,8,9} blob
// on the torso (nibbles of the shared RR1 rider palette line). Both are found per image
// from the pixels themselves, so every pose, mirror and in-between image is covered.
// D044 RR3 (port of tools/identity_sheets.py classify3): 1 helmet = top-most green {5,6,7} blob, first rows only;
// 2 vest = grey {8,9,10} nearer to a green than to a bike-only index {1,2,3,15}, grown by 8-connectivity;
// 3 suit = the other green blob pixels. Unsafe images (no bike, implausible helmet/vest: falls) stay all original.
void classify_roles3(const uint8_t *nib,int w,int h,std::vector<uint8_t>& out){
    out.assign((size_t)w*h,0);
    int top=-1,last=-1;
    for(int y=0;y<h&&top<0;++y)for(int x=0;x<w;++x)if(nib[y*w+x]){top=y;break;}
    for(int y=h-1;y>=0&&last<0;--y)for(int x=0;x<w;++x)if(nib[y*w+x]){last=y;break;}
    if(top<0)return;
    int fig=last-top+1;
    std::vector<std::vector<int>> greens;
    {std::vector<uint8_t> seen((size_t)w*h,0);std::vector<int> stack;
     for(int start=0;start<w*h;++start){
        if(seen[start]||nib[start]<5||nib[start]>7)continue;
        std::vector<int> blob;stack.assign(1,start);seen[start]=1;
        while(!stack.empty()){int at=stack.back();stack.pop_back();blob.push_back(at);int x=at%w,y=at/w;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int u=x+dx,t=y+dy;if(u<0||u>=w||t<0||t>=h)continue;
                int to=t*w+u;if(!seen[to]&&nib[to]>=5&&nib[to]<=7){seen[to]=1;stack.push_back(to);}}}
        if(blob.size()>=6)greens.push_back(std::move(blob));}}
    // Chebyshev distance (two passes) to any green pixel and to bike-only pixels.
    const int FAR=99;std::vector<int> dg((size_t)w*h,FAR),db((size_t)w*h,FAR);int bikes=0;
    for(int i=0;i<w*h;++i){if(nib[i]>=5&&nib[i]<=7)dg[i]=0;if(nib[i]==1||nib[i]==2||nib[i]==3||nib[i]==15){db[i]=0;++bikes;}}
    if(bikes<4||greens.empty())return;
    auto chebyshev=[&](std::vector<int>& d){
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){int& c=d[y*w+x];
            if(x>0)c=std::min(c,d[y*w+x-1]+1);if(y>0){c=std::min(c,d[(y-1)*w+x]+1);
                if(x>0)c=std::min(c,d[(y-1)*w+x-1]+1);if(x+1<w)c=std::min(c,d[(y-1)*w+x+1]+1);}}
        for(int y=h-1;y>=0;--y)for(int x=w-1;x>=0;--x){int& c=d[y*w+x];
            if(x+1<w)c=std::min(c,d[y*w+x+1]+1);if(y+1<h){c=std::min(c,d[(y+1)*w+x]+1);
                if(x+1<w)c=std::min(c,d[(y+1)*w+x+1]+1);if(x>0)c=std::min(c,d[(y+1)*w+x-1]+1);}}};
    chebyshev(dg);chebyshev(db);
    // Helmet: the green blob whose first row is highest (ties: larger).
    const std::vector<int> *head=nullptr;int headTop=1<<20;
    for(const auto& b:greens){int t=1<<20;for(int at:b)t=std::min(t,at/w);
        if(!head||t<headTop||(t==headTop&&b.size()>head->size())){head=&b;headTop=t;}}
    int limit=headTop+std::max(6,(int)(.16*fig));
    int bx0=1<<20,by0=1<<20,bx1=-1,by1=-1,hn=0;double cy=0;
    for(int at:*head){int x=at%w,y=at/w;if(y>=limit)continue;
        out[at]=1;++hn;cy+=y;bx0=std::min(bx0,x);by0=std::min(by0,y);bx1=std::max(bx1,x);by1=std::max(by1,y);}
    if(!hn||cy/hn-top>.4*fig){std::fill(out.begin(),out.end(),0);return;}
    bx0--;by0--;bx1++;by1++;
    std::vector<uint8_t> near((size_t)w*h,0),seen((size_t)w*h,0);std::vector<int> stack;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){int i=y*w+x;
        if(nib[i]>=8&&nib[i]<=10&&!(x>=bx0&&x<=bx1&&y>=by0&&y<=by1)&&dg[i]<=db[i]){near[i]=1;if(dg[i]<=8){seen[i]=1;stack.push_back(i);}}}
    int vest=0;
    while(!stack.empty()){int at=stack.back();stack.pop_back();out[at]=2;++vest;int x=at%w,y=at/w;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int u=x+dx,t=y+dy;if(u<0||u>=w||t<0||t>=h)continue;
            int to=t*w+u;if(near[to]&&!seen[to]){seen[to]=1;stack.push_back(to);}}}
    if(vest<12){std::fill(out.begin(),out.end(),0);return;}
    for(const auto& b:greens)for(int at:b)if(!out[at])out[at]=3;
}
void classify_roles(const uint8_t *nib,int w,int h,std::vector<uint8_t>& out){
    if(game_index==2){classify_roles3(nib,w,h,out);return;}
    out.assign((size_t)w*h,0);
    auto blobs=[&](unsigned mask){
        std::vector<std::vector<int>> found;std::vector<uint8_t> seen((size_t)w*h,0);std::vector<int> stack;
        for(int start=0;start<w*h;++start){
            if(seen[start]||!((mask>>nib[start])&1))continue;
            std::vector<int> blob;stack.assign(1,start);seen[start]=1;
            while(!stack.empty()){int at=stack.back();stack.pop_back();blob.push_back(at);
                int x=at%w,y=at/w;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int u=x+dx,t=y+dy;
                    if(u<0||u>=w||t<0||t>=h)continue;int to=t*w+u;
                    if(!seen[to]&&((mask>>nib[to])&1)){seen[to]=1;stack.push_back(to);}}}
            found.push_back(std::move(blob));
        }
        return found;};
    auto pick=[&](unsigned mask,double lo,double hi,uint8_t role,bool skip_marked){
        const std::vector<int> *best=nullptr;std::vector<int> kept,bestkept;
        auto all=blobs(mask);
        for(auto& blob:all){kept.clear();double cy=0;
            for(int at:blob)if(!skip_marked||!out[at]){kept.push_back(at);cy+=at/w;}
            if(kept.empty())continue;cy/=kept.size();
            if(cy>h*lo&&cy<h*hi&&(!best||kept.size()>bestkept.size())){bestkept=kept;best=&blob;}}
        for(int at:bestkept)out[at]=role;};
    pick(0x1e,-1,.35,1,false);   // nibbles 1,2,3,4
    pick(0x310,.06,.55,2,true);  // nibbles 4,8,9
    // D034 suit: every yellow blob (nibbles 5,6,7) of >=6 px not already helmet/vest; bike/boots use other nibbles.
    for(auto& blob:blobs(0xe0))if(blob.size()>=6)for(int at:blob)if(!out[at])out[at]=3;
}
const RoleMap *image_roles(const rr::SpriteAsset& image){
    auto& e=role_maps[&image];
    if(e.tick!=tick){e.tick=tick;uint32_t hash=2166136261u;
        for(uint8_t n:image.pixels)hash=(hash^n)*16777619u;
        if(e.w!=image.width||e.h!=image.height||e.hash!=hash||e.roles.empty()){
            e.w=image.width;e.h=image.height;e.hash=hash;
            if(image.pixels.size()==(size_t)image.width*image.height){
                classify_roles(image.pixels.data(),image.width,image.height,e.roles);
                std::vector<uint8_t> coded(image.pixels.size());
                for(size_t i=0;i<coded.size();++i)coded[i]=image.pixels[i]?(uint8_t)(image.pixels[i]+16*e.roles[i]):0;
                e.coded.build(image.width,image.height,coded.data());}
            else e.roles.assign((size_t)image.width*image.height,0);}}
    return &e;
}
const RoleMap *canvas_roles(const ActorCanvas& canvas){
    auto& e=role_maps[&canvas];
    if(e.tick!=tick){e.tick=tick;std::vector<uint8_t> nib(canvas.index.size(),0);uint32_t hash=2166136261u;
        for(size_t i=0;i<nib.size();++i){nib[i]=canvas.opaque[i]&&(canvas.index[i]>>4)==2?canvas.index[i]&15:0;
            hash=(hash^canvas.index[i]^(canvas.opaque[i]?256u:0u)^(canvas.high[i]?512u:0u))*16777619u;}
        if(e.w!=canvas.width||e.h!=canvas.height||e.hash!=hash||e.roles.empty()){
            e.w=canvas.width;e.h=canvas.height;e.hash=hash;classify_roles(nib.data(),canvas.width,canvas.height,e.roles);
            std::vector<uint8_t> coded(nib.size(),0);
            for(size_t i=0;i<coded.size();++i)if(canvas.opaque[i])
                coded[i]=(canvas.index[i]>>4)==2?(uint8_t)(nib[i]+16*e.roles[i]):(uint8_t)(64+(canvas.index[i]&63));
            e.coded.build(canvas.width,canvas.height,coded.data(),canvas.opaque.data(),canvas.high.data());}}
    return &e;
}
const RivalLook *rival_look(const std::string& name){   // null = keep the original look (police, unknown)
    // RR2 names have irregular case in the ROM ("ralph", "Rude Boy"): compare without case. RR2 own kits first, then the shared RR1 kits.
    auto same=[&](const char *a){size_t n=name.size();if(std::strlen(a)!=n)return false;
        for(size_t i=0;i<n;++i)if(std::tolower((unsigned char)a[i])!=std::tolower((unsigned char)name[i]))return false;return true;};
    if(game_index==2){
        for(const auto& look:rr3_looks)if(same(look.name))return &look;
        for(const auto& look:rr2_looks)if(same(look.name))return &look;   // P.E. No.1, Sven, Jorg
        for(const auto& look:rr1_looks)if(same(look.name))return &look;   // Natasha, Ikira, Biff, Axle, Sergio
        return nullptr;
    }
    if(game_index==1){
        for(const auto& look:rr2_looks)if(same(look.name))return &look;
        for(const auto& look:rr1_looks)if(same(look.name))return &look;   // Natasha, Slater, Rude Boy, Axle, Viper, Ikira, Biff = RR1 kits
        return nullptr;
    }
    for(const auto& look:rr1_looks)if(name==look.name)return &look;
    return nullptr;
}
// D036: which SAT slots belong to which rival, from the captured jobs (newest first, SAT must match exactly).
// Runs every frame whether or not the geometry scene is active. When no job explains a slot but its SAT bytes
// are unchanged since the previous frame (pause, a lapsed job), the previous owner is kept.
void identity_own(const GVDP *v){
    if(id_owner_tick==tick)return;id_owner_tick=tick;
    unsigned sat=(v->reg[5]&((v->reg[12]&1)?0x7e:0x7f))<<9;native_sat=sat;
    std::array<bool,80> linked{},taken{};unsigned link=0;
    for(unsigned n=0;n<80&&link<80&&!linked[link];++n){
        linked[link]=true;unsigned next=v->vram[(sat+link*8+3)&65535]&127;
        if(!next)break;link=next;
    }
    std::array<uint32_t,80> owner{},group{};
    std::vector<const Job*> sources;
    for(auto it=pending.rbegin();it!=pending.rend();++it)sources.push_back(&*it);
    for(auto it=id_archive.rbegin();it!=id_archive.rend();++it)sources.push_back(&*it);   // D038: jobs that outlived a scene reset
    for(const Job *source:sources){
        const Job& job=*source;
        if(!job.projection.actor||job.projection.player)continue;
        if(tick<job.tick||tick-job.tick>240||job.first<0||job.first>=80||!linked[job.first]||taken[job.first])continue;
        if(!matches(job,v,sat))continue;
        bool overlap=false;
        for(size_t n=0;n<job.pieces.size();++n)overlap|=taken[job.first+n]||!linked[job.first+n];
        if(overlap)continue;
        // One job can hold several riders far apart (the two HUD mirrors): split its pieces into horizontal clusters.
        std::vector<size_t> order(job.pieces.size());
        for(size_t n=0;n<order.size();++n)order[n]=n;
        auto left_of=[&](size_t n){return (int)(((job.pieces[n].sat[6]<<8|job.pieces[n].sat[7])&511)-128);};
        std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return left_of(a)<left_of(b);});
        unsigned cluster=0;int reach=-(1<<20);
        for(size_t k=0;k<order.size();++k){
            size_t n=order[k];int x=left_of(n),w=(((job.pieces[n].sat[2]>>2)&3)+1)*8;
            if(k&&x>reach+40)++cluster;
            reach=k?std::max(reach,x+w):x+w;
            owner[job.first+n]=job.projection.actor;group[job.first+n]=(uint32_t)job.first+1+cluster*100;taken[job.first+n]=true;
        }
    }
    for(unsigned s=0;s<80;++s){
        uint64_t sig=1469598103934665603ull;
        for(unsigned k=0;k<8;++k)if(k!=3)sig=(sig^v->vram[(sat+s*8+k)&65535])*1099511628211ull;   // every byte but the link: unchanged SAT entry (pause)
        if(!owner[s]&&linked[s]&&id_prev_owner[s]&&id_sig[s]==sig){owner[s]=id_prev_owner[s];group[s]=id_prev_group[s];}
        // D038: the game alternates two pattern banks and a scene reset empties the job list, so a bank shown again
        // after a gap has no job that explains it. The exact same SAT bytes in the same slot keep their earlier owner.
        {uint64_t key=(sig^(s+1))*1099511628211ull;
         if(owner[s]){if(id_memory.size()>4096)id_memory.clear();id_memory[key]={owner[s],group[s]};}
         else if(linked[s]){auto it=id_memory.find(key);if(it!=id_memory.end()){owner[s]=it->second.first;group[s]=it->second.second;}}}
        id_sig[s]=sig;
    }
    id_prev_owner=owner;id_prev_group=group;slot_actor=owner;slot_group=group;id_live=linked;
}
void native_roles_build(const GVDP *v,uint32_t actor,uint32_t group,NativeRoles& out){
    struct Cell {int x,y,w,h;unsigned attr,size,slot;};
    std::vector<Cell> cells;int left=1<<20,top=1<<20,right=-(1<<20),bottom=-(1<<20);
    auto byte=[&](unsigned at){return (unsigned)v->vram[at&65535];};
    for(unsigned s=0;s<80;++s){
        if(slot_actor[s]!=actor||slot_group[s]!=group||!id_live[s])continue;
        unsigned b=native_sat+s*8;
        Cell c;c.y=(int)(((byte(b)<<8)|byte(b+1))&511)-128;c.size=byte(b+2);c.attr=(byte(b+4)<<8)|byte(b+5);
        c.x=(int)(((byte(b+6)<<8)|byte(b+7))&511)-128;c.w=(((c.size>>2)&3)+1)*8;c.h=((c.size&3)+1)*8;c.slot=s;
        cells.push_back(c);left=std::min(left,c.x);top=std::min(top,c.y);right=std::max(right,c.x+c.w);bottom=std::max(bottom,c.y+c.h);
    }
    if(cells.empty()||right-left>256||bottom-top>256)return;
    out.left=left;out.top=top;out.w=right-left;out.h=bottom-top;
    std::vector<uint8_t> nib((size_t)out.w*out.h,0),opaque((size_t)out.w*out.h,0);
    for(const auto& c:cells){
        out.pieces.push_back({c.slot,c.x-left,c.y-top,c.w,c.h});
        unsigned bytes=0;const uint8_t *patterns=gvdp_sprite_patterns(c.size,&bytes);
        for(int y=0;y<c.h;++y)for(int x=0;x<c.w;++x){
            size_t dest=(size_t)(c.y-top+y)*out.w+(c.x-left+x);if(opaque[dest])continue;
            int u=(c.attr&0x0800)?c.w-1-x:x,w=(c.attr&0x1000)?c.h-1-y:y;
            unsigned tile=((c.attr&2047)+(u>>3)*(c.h/8)+(w>>3))&2047,at=tile*32+(w&7)*4+(u&7)/2;
            unsigned data=patterns?(at<bytes?patterns[at]:0):v->vram[at&65535];
            unsigned n=(data>>((u&1)?0:4))&15;if(!n)continue;
            opaque[dest]=1;nib[dest]=((c.attr>>13)&3)==2?(uint8_t)n:0;
        }
    }
    classify_roles(nib.data(),out.w,out.h,out.roles);
}
// Recolour the native sprite pixels owned by a rival (visible hardware sprites on this row).
void recolor_native(const GVDPSpriteLayer *layer){
    if(!layer->owner)return;
    for(const auto& item:native_roles){
        const auto *look=identity_of((uint32_t)item.first);if(!look)continue;
        const auto& r=item.second;
        for(const auto& piece:r.pieces){
            // The piece is shown where the geometry layer puts it (scaled/shifted rect), not where the SAT says.
            auto b=bounds[piece.slot];
            if(!b.valid)b={(double)(r.left+piece.cx),(double)(r.top+piece.cy),(double)(r.left+piece.cx+piece.w),(double)(r.top+piece.cy+piece.h),true};   // D035: unreplaced native piece
            if(b.x1<=b.x0||b.y1<=b.y0)continue;
            if(layer->line+.5<b.y0||layer->line+.5>=b.y1)continue;
            int ty=std::clamp((int)std::floor((layer->line+.5-b.y0)*piece.h/(b.y1-b.y0)),0,piece.h-1);
            int begin=std::max(0,(int)std::ceil(b.x0+layer->offset-.5)),end=std::min(layer->total,(int)std::ceil(b.x1+layer->offset-.5));
            for(int px=begin;px<end;++px){
                if(!layer->opaque[px]||layer->owner[px]!=piece.slot+1)continue;
                uint8_t index=layer->index[px];if((index>>4)!=2)continue;
                int tx=std::clamp((int)std::floor((px+.5-layer->offset-b.x0)*piece.w/(b.x1-b.x0)),0,piece.w-1);
                unsigned role=r.roles[(size_t)(piece.cy+ty)*r.w+(piece.cx+tx)];if(!role)continue;
                unsigned slot=(*look)[(role-1)*16+(index&15)];
                if(slot){layer->index[px]=(uint8_t)(GVDP_HOST_PALETTE_BASE+slot-1);}
            }
        }
    }
}
// Once per frame: assign every visible rival its helmet/vest host-palette entries.
void identity_build(const GVDP *v){
    identity_own(v);
    identity_slots_tick=tick;identity_map.clear();identity_entries.clear();native_roles.clear();
    identity_blocks.clear();identity_block_looks.clear();
    std::vector<uint32_t> actors;   // every rival with live sprites or a geometry drawing this frame
    for(uint32_t actor:slot_actor)if(actor&&std::find(actors.begin(),actors.end(),actor)==actors.end())actors.push_back(actor);
    for(const auto& drawing:actor_drawings)if(std::find(actors.begin(),actors.end(),drawing.actor)==actors.end())actors.push_back(drawing.actor);
    for(uint32_t actor:actors){
        struct {uint32_t actor;} drawing{actor};
        if(identity_map.count(drawing.actor))continue;
        std::string name;if(!rival_name(drawing.actor,name))continue;
        const RivalLook *found_look=rival_look(name);
        {static const bool trace=std::getenv("RR_IDENTITY_TRACE")!=nullptr;static std::map<std::string,int> seen_names;
         if(trace&&seen_names[name]++==0)std::fprintf(stderr,"[IDENTITY] tick=%llu actor=%06X name=%s kit=%s\n",(unsigned long long)tick,actor,name.c_str(),found_look?"yes":"NO (original)");}
        if(!found_look)continue;
        RivalLook look=*found_look;std::array<uint8_t,48> slots{};bool full=false;
        // One composed bitmap per job group (forward rider, near rider, each mirror copy are far apart on screen).
        std::vector<std::pair<uint64_t,NativeRoles>> groups;std::vector<uint32_t> done;
        for(unsigned s=0;s<80;++s){
            if(slot_actor[s]!=actor||!id_live[s]||std::find(done.begin(),done.end(),slot_group[s])!=done.end())continue;
            done.push_back(slot_group[s]);NativeRoles r;native_roles_build(v,actor,slot_group[s],r);
            if(!r.pieces.empty())groups.push_back({((uint64_t)slot_group[s]<<32)|actor,std::move(r)});
        }
        // Host palette entries (63 max) only matter for native SAT pixels or the no-coverage fallback.
        const bool need_host=!coverage_enabled||!groups.empty();
        auto entry=[&](uint8_t role,uint8_t nib,uint32_t target)->uint8_t{
            for(size_t i=0;i<identity_entries.size();++i){const auto& e=identity_entries[i];
                if(e.role==role&&e.nib==nib&&e.target==target)return (uint8_t)(i+1);}
            if(identity_entries.size()>=GVDP_HOST_PALETTE_SIZE){full=true;return 0;}
            identity_entries.push_back({target,role,nib});return (uint8_t)identity_entries.size();};
        if(need_host&&game_index==2){
            for(uint8_t nib:{5,6,7})slots[nib]=entry(1,nib,look.helmet);
            for(uint8_t nib:{8,9,10})slots[16+nib]=entry(2,nib,look.vest);
            for(uint8_t nib:{5,6,7})slots[32+nib]=entry(3,nib,look.suit);}
        else if(need_host){for(uint8_t nib:{1,2,3,4})slots[nib]=entry(1,nib,look.helmet);
        for(uint8_t nib:{4,8,9})slots[16+nib]=entry(2,nib,look.vest);
        for(uint8_t nib:{5,6,7})slots[32+nib]=entry(3,nib,look.suit);}
        (void)full;   // host palette is only the RR_SPRITE_COVERAGE=0 fallback; the coverage block below never depends on it
        identity_map[drawing.actor]=slots;
        if(identity_block_looks.size()<MAX_IDENTITY_BLOCKS){
            identity_blocks[drawing.actor]=(int)identity_block_looks.size();identity_block_looks.push_back(look);}
        for(auto& g:groups)native_roles[g.first]=std::move(g.second);
    }
}
// Helmet shades follow the rider's blue ramp (1 main, 2 darker, 3 darkest, 4 highlight); vest shades follow
// the white ramp (8 main, 9 stripe, 4 accent). `fade` is the live brightness of the rider palette line.
uint32_t identity_color(uint32_t target,unsigned role,unsigned nib,const double *fade){
    double c[3]={(double)((target>>16)&255),(double)((target>>8)&255),(double)(target&255)},f=1;
    if(game_index==2){   // D044 RR3: helmet and suit share the green ramp (6 main, 5 highlight, 7 dark); vest = grey 8/9/A
        if(role==2)f=nib==8?1.:(nib==9?.85:.7);
        else{f=nib==6?1.:.55;if(nib==5){for(double& x:c)x+=(255-x)*.45;f=1;}}}
    else if(role==1){static const double shade[5]={0,1,.66,.38,0};f=shade[nib&7];
        if(nib==4){for(double& x:c)x+=(255-x)*.45;f=1;}}
    else if(role==3){f=nib==6?1.:.55;if(nib==5){for(double& x:c)x+=(255-x)*.45;f=1;}}
    else f=nib==8?1.:(nib==9?.75:.55);
    // D037: shade, then the live per-channel fade, then snap to the Mega Drive DAC ladder (3 bits per channel),
    // so the result is a colour the original palette can actually show.
    static const double ladder[8]={0,49,87,119,146,174,206,255};
    uint32_t out=0xff000000u;
    for(int k=0;k<3;++k){
        double x=c[k]*f*fade[k];int best=0;
        for(int i=1;i<8;++i)if(std::fabs(ladder[i]-x)<std::fabs(ladder[best]-x))best=i;
        out|=(uint32_t)ladder[best]<<(16-8*k);
    }
    return out;
}
// Per-channel brightness of the rider line (white entry, CRAM 40 = $EEE at full brightness); follows the
// game's own fades, including the tinted fade-in, never its hue.
void identity_fade(const GVDP *v,double *fade){
    unsigned c=v->cram[40];const double ref=game_index==2?6.:7.;   // RR3 rider-line white is $CCC, not $EEE
    fade[0]=std::min(1.,((c>>1)&7)/ref);fade[1]=std::min(1.,((c>>5)&7)/ref);fade[2]=std::min(1.,((c>>9)&7)/ref);
}// Per scanline: the entry colours follow the live CRAM fade of the rider line, never its hue.
void identity_palette(const GVDP *v){
    if(identity_entries.empty())return;
    double fade[3];identity_fade(v,fade);
    std::array<uint32_t,GVDP_HOST_PALETTE_SIZE> palette;palette.fill(0xff000000);
    for(size_t i=0;i<identity_entries.size();++i){const auto& e=identity_entries[i];palette[i]=identity_color(e.target,e.role,e.nib,fade);}
    gvdp_set_host_palette(palette.data(),(unsigned)identity_entries.size());
}
// Coverage resolver palette: one block per rival at COVERAGE_BLOCK_BASE+k*COVERAGE_BLOCK_SIZE:
// [0..15] line-2 nibbles, [16..31] helmet, [32..47] vest, [48..111] CRAM 0..63 (all from the live CRAM).
void identity_blocks_fill(const GVDP *v,uint32_t *palette){
    double fade[3];identity_fade(v,fade);
    for(size_t k=0;k<identity_block_looks.size();++k){
        uint32_t *block=palette+COVERAGE_BLOCK_BASE+k*COVERAGE_BLOCK_SIZE;
        for(unsigned n=0;n<16;++n){uint32_t native=palette[32+n];block[n]=native;block[16+n]=native;block[32+n]=native;block[48+n]=native;}
        if(game_index==2){
            for(unsigned n:{5u,6u,7u})block[16+n]=identity_color(identity_block_looks[k].helmet,1,n,fade);
            for(unsigned n:{8u,9u,10u})block[32+n]=identity_color(identity_block_looks[k].vest,2,n,fade);
            for(unsigned n:{5u,6u,7u})block[48+n]=identity_color(identity_block_looks[k].suit,3,n,fade);
        }else{
        for(unsigned n:{1u,2u,3u,4u})block[16+n]=identity_color(identity_block_looks[k].helmet,1,n,fade);
        for(unsigned n:{4u,8u,9u})block[32+n]=identity_color(identity_block_looks[k].vest,2,n,fade);
        for(unsigned n:{5u,6u,7u})block[48+n]=identity_color(identity_block_looks[k].suit,3,n,fade);}
        for(unsigned i=0;i<64;++i)block[64+i]=palette[i];
    }
}
const std::array<uint8_t,48> *identity_of(uint32_t actor){
    auto found=identity_map.find(actor);return found==identity_map.end()?nullptr:&found->second;
}

void draw_actors(const GVDPSpriteLayer *layer,const GVDP *v){
    if(rival_identity&&!(v->reg[12]&8)){
        if(identity_slots_tick!=tick)identity_build(v);
        identity_palette(v);
        recolor_native(layer);
    }
    for(const auto& drawing:actor_drawings){
        const auto& cache=*drawing.cache;const auto& art=cache.canvas;double ratio=drawing.ratio;
        unsigned *pixels=pixel_trace?&vehicle_pixels[drawing.actor]:nullptr;
        if(layer->line+.5>=drawing.clip)continue;
        // Whole-row displacement only: every row keeps its own texels, width,
        // transparency, priority, hill clip and viewport limits.
        const double shift=drawing.row_shift(layer->line);
        const std::array<uint8_t,48> *look=rival_identity?identity_of(drawing.actor):nullptr;
        const ActorArt *part=drawing.morph?drawing.morph:cache.parts.data();
        const size_t part_count=drawing.morph?1:cache.parts.size();
        if(drawing.morph||(arcade_motion()&&!cache.parts.empty()&&(!cache.focal||
           coverage_enabled||(drawing.root.scale>=.25&&art.height*ratio>=16&&art.width*ratio>=8)))){
            for(size_t part_index=0;part_index<part_count;++part_index){const auto& p=part[part_index];
                const auto& image=*p.art;const auto& s=p.source;
                double scale=s.scale*ratio,x=drawing.root.x+(s.x-cache.root.x)*ratio;
                double y=drawing.root.y+(s.y-cache.root.y)*ratio;
                int ax=(p.attr&0x0800)?image.width-image.anchor_x:image.anchor_x;
                int ay=(p.attr&0x1000)?image.height-image.anchor_y:image.anchor_y;
                double left=x-ax*scale+layer->offset,top=y-ay*scale;
                if(shift!=0)left+=shift;
                int w=(int)std::floor((layer->line+.5-top)/scale);
                if(w<0||w>=image.height)continue;
                if(p.attr&0x1000)w=image.height-1-w;
                const RoleMap *roles=look?image_roles(image):nullptr;
                int begin=std::max(std::max(0,left_edge()+layer->offset),(int)std::ceil(left-.5));
                int end=std::min(std::min(layer->total,right_edge()+layer->offset),(int)std::ceil(left+image.width*scale-.5));
                for(int px=begin;px<end;++px){
                    if(layer->opaque[px]&&(!layer->owner||layer->owner[px]<=drawing.order))continue;
                    int u=(int)std::floor((px+.5-left)/scale);if(u<0||u>=image.width)continue;
                    if(p.attr&0x0800)u=image.width-1-u;
                    unsigned nib=image.pixels[w*image.width+u];if(!nib)continue;
                    uint8_t shown=(uint8_t)(p.base+nib);
                    if(look&&p.base==32){unsigned role=roles->roles[w*image.width+u];   // palette line 2 only
                        if(role){unsigned slot=(*look)[(role-1)*16+nib];if(slot){shown=(uint8_t)(GVDP_HOST_PALETTE_BASE+slot-1);}}}
                    layer->index[px]=shown;layer->high[px]=(p.attr>>15)&1;layer->opaque[px]=1;
                    if(layer->owner)layer->owner[px]=(uint8_t)drawing.order;
                    if(pixels)++*pixels;
                }
            }
            continue;
        }
        double left=drawing.root.x+(art.x-cache.root.x)*ratio+layer->offset;
        if(shift!=0)left+=shift;
        double top=drawing.root.y+(art.y-cache.root.y)*ratio;
        int y=(int)std::floor((layer->line+.5-top)/ratio);if(y<0||y>=art.height)continue;
        int begin=std::max(std::max(0,left_edge()+layer->offset),(int)std::ceil(left-.5));
        int end=std::min(std::min(layer->total,right_edge()+layer->offset),(int)std::ceil(left+art.width*ratio-.5));
        for(int x=begin;x<end;++x){
            if(layer->opaque[x]&&(!layer->owner||layer->owner[x]<=drawing.order))continue;
            int u=(int)std::floor((x+.5-left)/ratio);if(u<0||u>=art.width)continue;
            size_t at=(size_t)y*art.width+u;if(!art.opaque[at])continue;
            uint8_t shown=art.index[at];
            if(look&&(shown>>4)==2){unsigned role=canvas_roles(art)->roles[at];
                if(role){unsigned slot=(*look)[(role-1)*16+(shown&15)];if(slot){shown=(uint8_t)(GVDP_HOST_PALETTE_BASE+slot-1);}}}
            layer->index[x]=shown;layer->high[x]=art.high[at];layer->opaque[x]=1;
            if(layer->owner)layer->owner[x]=(uint8_t)drawing.order;
            if(pixels)++*pixels;
        }
    }
}

void draw_objects(void*,const GVDP *v,const GVDPSpriteLayer *layer){
    if(rival_identity&&!(v->reg[12]&8)){   // D036: identity does not depend on the geometry scene (start fade, pause, mirrors)
        if(identity_slots_tick!=tick)identity_build(v);
        if(!active||!row_inside(layer->line)){identity_palette(v);recolor_native(layer);}
    }
    if(!organic||!active||!row_inside(layer->line))return;
    draw_actors(layer,v);
    for(const auto& b:billboards){
        int ax=(b.flags&0x8000)?b.art->width-b.art->anchor_x:b.art->anchor_x;
        int ay=(b.flags&0x4000)?b.art->height-b.art->anchor_y:b.art->anchor_y;
        double left=b.pose.x-ax*b.pose.scale+layer->offset;
        double top=b.pose.y-ay*b.pose.scale;
        int y=(int)std::floor((layer->line+.5-top)/b.pose.scale);
        if(y<0||y>=b.art->height||layer->line+.5>=b.clip)continue;
        if(b.flags&0x4000)y=b.art->height-1-y;
        int begin=std::max(std::max(0,left_edge()+layer->offset),(int)std::ceil(left-.5));
        int end=std::min(std::min(layer->total,right_edge()+layer->offset),(int)std::ceil(left+b.art->width*b.pose.scale-.5));
        for(int x=begin;x<end;++x){
            if(layer->opaque[x]&&(!layer->owner||layer->owner[x]<=b.order))continue;
            int u=(int)std::floor((x+.5-left)/b.pose.scale);
            if(b.flags&0x8000)u=b.art->width-1-u;
            if(u<0||u>=b.art->width)continue;
            unsigned nib=b.art->pixels[y*b.art->width+u];if(!nib)continue;
            layer->index[x]=(uint8_t)(b.palette*16+nib);
            layer->opaque[x]=1;layer->high[x]=(uint8_t)b.priority;
            if(layer->owner)layer->owner[x]=(uint8_t)b.order;
        }
    }
}

void prepare(const GVDP *v,unsigned sat){
    for(auto& item:bounds)item.valid=false;
    player_slots.fill(false);
    if(!enabled||!active)return;
    std::array<bool,80> used{};
    std::array<bool,80> linked{};
    std::array<uint32_t,80> actor_slots{};
    std::map<std::pair<uint32_t,uint64_t>,std::vector<const Job*>> actor_groups;
    unsigned link=0;
    for(unsigned n=0;n<80&&link<80&&!linked[link];++n){
        linked[link]=true;unsigned next=v->vram[(sat+link*8+3)&65535]&127;
        if(!next)break;link=next;
    }
    live_slots=linked;
    native_slots=linked;
    if(organic)find_markings(v,sat,linked);
    if(organic){billboards.clear();actor_drawings.clear();replaced.fill(false);}
    for(auto it=pending.rbegin();it!=pending.rend();++it){
        const Job& job=*it;
        if(tick<job.tick||tick-job.tick>40||job.first<0||job.first>=80||!linked[job.first]||used[job.first]||
           !matches(job,v,sat,!(coverage_enabled&&job.projection.scenery)))continue;
        // Rear-view mirrors reuse the same actor IDs as the forward camera.
        // Their two HUD copies must never become a forward actor assembly.
        if(job.projection.actor&&!job.projection.player&&job.projection.z<job.projection.camera&&job.y>=view_bottom)continue;
        bool overlap=false;
        for(size_t n=0;n<job.pieces.size();++n)overlap|=used[job.first+n]||!linked[job.first+n];
        if(overlap)continue;
        double x=job.x;
        double first_x=((job.pieces[0].sat[6]<<8|job.pieces[0].sat[7])&511)-128;
        // RR1 intentionally keeps signed projected coordinates until SAT wrap.
        while(x<first_x-192)x+=512;
        while(x>first_x+192)x-=512;
        if(organic)x=job.x+(game_index==0&&(job.projection.scenery||job.x<0)?512.:0.);
        auto& h=histories[job.key];
        if(h.serial!=job.serial){
            Pose next{x,job.y,job.scale};
            bool same_frame=h.serial&&h.changed==tick;
            uint64_t distance=h.serial?tick-h.changed:0;
            double dx=next.x-h.current.x,dy=next.y-h.current.y;
            bool world_object=organic&&job.projection.scenery;
            bool continuous=same_frame||(h.serial&&distance>=1&&distance<=12&&tick-h.seen<=16&&
                (h.image==job.image||world_object)&&((h.flags^job.flags)&0xf000)==0&&
                std::abs(dx)<(world_object?320:64)&&std::abs(dy)<(world_object?160:32)&&
                next.scale/h.current.scale>(world_object?.15:.55)&&next.scale/h.current.scale<(world_object?6.:1.8));
            if(!same_frame){h.previous=continuous?h.current:next;h.duration=continuous?(double)distance:1.;}
            h.current=next;
            h.interpolate=continuous;h.changed=tick;h.serial=job.serial;h.image=job.image;h.flags=job.flags;
            if(!continuous)++resets;
            if(organic){
                if(!continuous)h.samples.clear();
                if(same_frame&&!h.samples.empty()&&h.samples.back().first==tick)h.samples.back().second=next;
                else h.samples.emplace_back(tick,next);
                while(h.samples.size()>12)h.samples.pop_front();
            }
        }
        h.seen=tick;
        h.art=job;
        double alpha=h.interpolate?std::clamp((tick-h.changed+1.)/h.duration,0.,1.):1.;
        Pose pose=sample(h,alpha);
        if(organic&&!h.samples.empty()){
            pose=scene_sample(h);
            // The player's complete bike/attack pose is already camera-sized.
            // Never extrapolate its input response or independent body parts.
            if(job.projection.player||(!job.projection.actor&&!job.projection.scenery&&std::abs(job.scale-1.)<1e-9))
                pose={x,job.y,job.scale};
            if(!job.projection.scenery&&(!std::isfinite(pose.scale)||pose.scale<=0||
                pose.scale/job.scale<.5||pose.scale/job.scale>2.))pose={x,job.y,job.scale};
        }
        bool full=organic&&add_billboards(h,false);
        scenery_audit(v,job,{x,job.y,job.scale},full);
        if(organic&&job.projection.scenery&&!full)pose={x,job.y,job.scale};
        double ratio=pose.scale/job.scale;
        ++matched;if(alpha<1)++intermediate;
        if(trace)std::fprintf(trace,"%llu,%016llX,%u,%.6f,%.6f,%.9f,%.6f,%.6f,%.9f,%.6f,%06X,%u,%llu\n",
            (unsigned long long)tick,(unsigned long long)job.key,job.image,x,job.y,job.scale,pose.x,pose.y,pose.scale,alpha,
            job.projection.actor,job.projection.component,(unsigned long long)job.projection.epoch);
        if(organic&&job.projection.actor&&!job.projection.player)
            actor_groups[{job.projection.actor,job.projection.epoch}].push_back(&job);
        for(size_t n=0;n<job.pieces.size();++n){
            const auto& piece=job.pieces[n];unsigned slot=job.first+(unsigned)n;
            int px=((piece.sat[6]<<8|piece.sat[7])&511)-128;
            int py=((piece.sat[0]<<8|piece.sat[1])&511)-128;
            int w=(((piece.sat[2]>>2)&3)+1)*8,hgt=((piece.sat[2]&3)+1)*8;
            bounds[slot]={pose.x+(px-x)*ratio,pose.y+(py-job.y)*ratio,
                pose.x+(px+w-x)*ratio,pose.y+(py+hgt-job.y)*ratio,true};used[slot]=true;
            replaced[slot]=full;
            actor_slots[slot]=job.projection.actor;
            native_slots[slot]=!full&&std::abs(bounds[slot].x0-px)<1e-7&&
                std::abs(bounds[slot].y0-py)<1e-7&&std::abs(bounds[slot].x1-px-w)<1e-7&&
                std::abs(bounds[slot].y1-py-hgt)<1e-7;
            constexpr unsigned player_jobs[]={0xffd37c,0xffceb8,0xff371c};
            player_slots[slot]=job.projection.player||(!view_id&&game_index>=0&&
                job.address==player_jobs[game_index]&&std::abs(job.scale-1.)<1e-9);
        }
        // Prototype 03: the player's rider+bike assembly (RR1 forward view only).
        if((pose_player||pose_dissolving||player_jobs_trace)&&organic&&!view_id&&game_index==0&&job.projection.player){
            if(player_jobs_trace)std::fprintf(player_jobs_trace,"%llu,%06X,%u,%04X,%u,%.1f,%.1f,%.4f,%d,%u,%u,%u,%llu,%02X%02X\n",
                (unsigned long long)tick,job.address,job.image,job.flags,job.second,x,job.y,job.scale,job.first,
                (unsigned)job.pieces.size(),job.projection.components,job.projection.component,
                (unsigned long long)job.serial,job.pieces[0].sat[4],job.pieces[0].sat[5]);
            if(pose_dissolving&&job.projection.components==1&&std::abs(job.scale-1.)<1e-9&&track_player_dissolve(v,job,x)){
                actor_drawings.push_back({&player_cache,Pose{x,job.y,1.},1.,(unsigned)job.first+1,false,job.clip,(uint32_t)view.player});
                actor_drawings.back().morph=&player_cache.dissolve.part;
                for(size_t n=0;n<job.pieces.size();++n){replaced[job.first+n]=true;native_slots[job.first+n]=false;}
            }
            else if(pose_player&&job.projection.components==1&&std::abs(job.scale-1.)<1e-9&&track_player(v,job,x)){
                actor_drawings.push_back({&player_cache,Pose{x,job.y,1.},1.,(unsigned)job.first+1,false,job.clip,(uint32_t)view.player});
                actor_drawings.back().morph=&player_cache.morph.part;++pose_ease_frames;
                for(size_t n=0;n<job.pieces.size();++n){replaced[job.first+n]=true;native_slots[job.first+n]=false;}
            }
        }
    }
    native_sat=sat;
    if(organic){
        std::map<uint32_t,std::vector<const Job*>> complete;
        for(auto& item:actor_groups){
            auto& jobs=item.second;unsigned expected=jobs.front()->projection.components;
            if(jobs.size()!=expected)continue;
            std::sort(jobs.begin(),jobs.end(),[](const Job*a,const Job*b){return a->projection.component<b->projection.component;});
            bool valid=true;for(unsigned i=0;i<expected;++i)valid&=jobs[i]->projection.component==i;
            if(!valid)continue;
            auto& chosen=complete[item.first.first];
            if(chosen.empty()||chosen[0]->projection.epoch<item.first.second)chosen=jobs;
        }
        for(auto& item:complete){
            const auto& jobs=item.second;const Job& primary=*jobs[0];auto& h=histories[primary.key];
            auto& cache=actor_caches[item.first];
            cache.actor=item.first;
            if(cache.epoch!=primary.projection.epoch){
                ActorCanvas canvas;
                const auto *packet=source_packet(item.first,primary.projection.epoch);
                if(packet&&rear_view(*packet))continue;
                bool canonical=packet&&!packet->parts.empty();
                bool source=edge_recovery()&&canonical;
                Pose root=canonical?Pose{packet->parts[0].x,packet->parts[0].y,packet->parts[0].scale}:h.current;
                std::vector<ActorArt> parts;
                if(!compose_actor(v,jobs,root,canvas,&cache,source?packet:nullptr,&parts))continue;
                cache.canvas=std::move(canvas);cache.root=root;cache.root_key=primary.key;cache.full_source=source;
                cache.parts=std::move(parts);
                commit_pose(cache,canonical?packet:nullptr);
                cache.has_motion=canonical;
                if(canonical)source_motion(cache,*packet);
                if(source)++actor_source_assemblies;
                cache.epoch=primary.projection.epoch;cache.first=80;
                for(const Job *job:jobs)cache.first=std::min(cache.first,(unsigned)job->first);
            }
            cache.seen=tick;
        }
        if(edge_recovery())for(const auto& item:actor_sources){
            if(item.second.empty())continue;const auto& packet=item.second.back();auto plan=actor_plans.find(item.first);
            if(packet.parts.empty()||rear_view(packet)||tick-packet.tick>18||plan==actor_plans.end()||plan->second.components||plan->second.epoch!=packet.epoch)continue;
            auto& cache=actor_caches[item.first];cache.actor=item.first;source_motion(cache,packet);
            if(cache.epoch!=packet.epoch){
                const auto& p=packet.parts[0];Pose root{p.x,p.y,p.scale};ActorCanvas canvas;
                std::vector<ActorArt> parts;
                if(compose_actor(v,{},root,canvas,&cache,&packet,&parts)){cache.canvas=std::move(canvas);cache.parts=std::move(parts);cache.root=root;cache.full_source=true;cache.has_motion=true;++actor_source_only;
                    commit_pose(cache,&packet);}
                cache.epoch=packet.epoch;
            }
            cache.seen=tick;
        }
        for(auto it=actor_caches.begin();it!=actor_caches.end();){
            auto& cache=it->second;uint64_t age=tick-cache.seen;
            auto sources=actor_sources.find(it->first);
            const SourcePacket *latest=sources!=actor_sources.end()&&!sources->second.empty()?&sources->second.back():nullptr;
            double depth=latest?2.+latest->world-latest->camera:0;
            auto hist=histories.find(cache.root_key);
            // A far native assembly may precede any usable source snapshot.
            // Recover only its far-distance calibration from the committed
            // bitmap and a verified live actor; never use this for near poses.
            if(!cache.focal&&latest&&latest->alive&&depth>=28&&depth<=34&&hist!=histories.end()){
                double f=cache.root.scale*depth;
                if(std::abs(f-2.5*view_focal)<.08)cache.focal=2.5*view_focal;
                else if(std::abs(f-3.*view_focal)<.08)cache.focal=3.*view_focal;
                if(cache.focal){
                    cache.appearance=*latest;const auto& job=hist->second.art;
                    cache.appearance.parts={{cache.root.x,cache.root.y,cache.root.scale,job.clip,job.image,job.second,job.flags,job.address}};
                }
            }
            // A live vehicle crossing the native far-distance cutoff must not
            // flash as braking moves it repeatedly across that one threshold.
            // This separate, bounded far band never authorizes near removal.
            bool alive_now=source_ram&&(source_ram[((it->first&65535)+104)&65535]&0x40)!=0;
            bool far_active=latest&&latest->alive&&alive_now&&tick-latest->tick<=12&&depth>=28&&depth<=34&&
                cache.focal>0&&!cache.appearance.parts.empty();
            bool near_active=latest&&latest->alive&&alive_now&&tick-latest->tick<=12&&depth>=.25&&depth<2.5&&
                cache.world_height_valid&&!cache.appearance.parts.empty()&&tick-cache.appearance.tick<=32;
            // An empty native builder is also possible between completed
            // uploads on a crest/turn, not just at the far cutoff. Bridge a
            // short mid-distance gap only for the SAME live render state;
            // the current hill mask still decides whether any pixels show.
            unsigned state_now=source_ram?((source_ram[((it->first&65535)+104)&65535]<<8)|
                source_ram[((it->first&65535)+110)&65535]):0;
            bool live_gap=latest&&latest->parts.empty()&&latest->alive&&alive_now&&tick-latest->tick<=12&&
                depth>=2.5&&depth<28&&cache.world_height_valid&&cache.focal>0&&
                !cache.appearance.parts.empty()&&tick-cache.appearance.tick<=12&&
                latest->render_state==cache.appearance.render_state&&state_now==latest->render_state;
            // A completed departure cannot fall back to a legacy forward
            // history on a later mirror upload. Only a new forward projection
            // may make that actor enter the main camera again.
            if(latest&&rear_view(*latest)&&!near_active){++it;continue;}
            if(age>40&&!far_active&&!near_active){it=actor_caches.erase(it);continue;}
            auto plan=actor_plans.find(it->first);
            bool held=age!=0;
            bool fresh=sources!=actor_sources.end()&&!sources->second.empty()&&
                !sources->second.back().parts.empty()&&!rear_view(sources->second.back())&&tick-sources->second.back().tick<=18;
            // Position belongs to the actor, not its changing animation image.
            // Move the last complete assembly with the latest descriptor even
            // while a new texture upload is incomplete; never mix body parts.
            if(cache.has_motion&&fresh)source_motion(cache,sources->second.back());
            bool source=cache.full_source&&sources!=actor_sources.end()&&!sources->second.empty()&&
                !sources->second.back().parts.empty()&&!rear_view(sources->second.back())&&tick-sources->second.back().tick<=18&&!cache.motion.samples.empty();
            if(near_active||far_active||live_gap||source||(hist!=histories.end()&&age<=12&&(!held||(plan!=actor_plans.end()&&plan->second.components&&tick-plan->second.tick<=18)))){
                bool motion=cache.has_motion&&fresh&&!cache.motion.samples.empty();
                // A new empty builder packet can precede removal of the old
                // SAT upload. While that complete assembly is still present,
                // keep its pose on the current road instead of reverting to
                // the older bitmap history. Retention ends with native SAT.
                bool native_hold=cache.has_motion&&cache.focal>0&&age==0&&sources!=actor_sources.end()&&
                    !sources->second.empty()&&tick-sources->second.back().tick<=12&&
                    sources->second.back().world>0&&sources->second.back().camera>0;
                double clip=(double)view_bottom,display_depth=0;bool hill_anchor=false;
                bool world_model=cache.focal>0&&latest&&latest->camera>0&&latest->world>0&&!cache.appearance.parts.empty();
                Pose root=(motion||native_hold||far_active||near_active||live_gap||world_model)?(arcade_motion()?vehicle_sample(cache,sources->second,clip,display_depth,hill_anchor):cache.motion.current):scene_sample(hist->second);
                if(!std::isfinite(root.scale)||root.scale<=0)root=motion?cache.motion.current:cache.root;
                if(cache.focal>0)root.scale=std::min(root.scale,cache.focal/(cache.raw_height?.25:2.5));
                if(root.scale>0){
                    double gain=1.;
                    double ratio=root.scale/cache.root.scale*gain;
                    if(ratio>0&&ratio<4096){
                        actor_drawings.push_back({&cache,root,ratio,cache.first+1,held,clip,it->first});
                        ++actor_canvas_frames;if(held)++actor_hold_frames;
                        if(pose_easing&&cache.ease.active){
                            // The residual belongs to a rigid rider only. A
                            // crash or unknown projection ends it immediately.
                            double share=cache.focal>0?pose_ease_factor(cache.ease,tick):0;
                            if(share>0){
                                auto& drawing=actor_drawings.back();
                                drawing.row_offset=cache.ease.offset*share*root.scale;drawing.row_slope=cache.ease.slope*share;
                                ++pose_ease_frames;
                                if(pose_trace)std::fprintf(pose_trace,"%llu,%06X,%llu,%u,%.4f,%.4f,%.5f,%.5f,%.5f,0\n",
                                    (unsigned long long)tick,it->first,(unsigned long long)cache.epoch,cache.ease.image,
                                    cache.ease.duration,share,drawing.row_offset,drawing.row_slope,root.scale);
                            }else cache.ease.active=false;
                        }
                        if(pose_morphing&&cache.morph.active){
                            // Rigid riders only; far specks keep the native swap.
                            if(cache.focal>0&&build_morph(cache)){
                                if(root.scale>=.2){
                                    actor_drawings.back().morph=&cache.morph.part;++pose_ease_frames;
                                    if(pose_trace){const auto& m=cache.morph;
                                        std::fprintf(pose_trace,"%llu,%06X,%llu,%u,%.4f,%.4f,%.5f,%.5f,%.5f,%.5f\n",
                                            (unsigned long long)tick,it->first,(unsigned long long)cache.epoch,m.to.image,m.duration,
                                            1.-pose_curve(pose_progress(m,tick)),pose_distance(m.start,m.to.shape)*root.scale,0.,root.scale,
                                            std::abs(m.start.height-m.to.shape.height)*root.scale);}
                                }
                            }else cache.morph.active=false;
                        }
                        if(pose_following&&cache.morph.active){
                            // Prototype 03. Rigid riders only; far specks keep the native swap.
                            Pose source=cache.root;
                            if(cache.focal>0&&root.scale>=.2&&build_follow(cache.morph,source)){
                                actor_drawings.back().morph=&cache.morph.part;++pose_ease_frames;
                                if(pose_trace){const auto& m=cache.morph;double box[4];
                                    Pose at{root.x,root.y,root.scale};follow_box(m,at,box);
                                    double gap=pose_gap(m.shown,m.to.shape);
                                    std::fprintf(pose_trace,"%llu,%06X,%llu,%u,%.4f,%.4f,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f,%.2f,%.2f\n",
                                        (unsigned long long)tick,it->first,(unsigned long long)cache.epoch,m.to.image,m.omega,gap,
                                        gap*root.scale,0.,root.scale,std::abs(m.shown.height-m.to.shape.height)*root.scale,
                                        box[0],box[1],box[2],box[3]);}
                            }else if(!cache.morph.active||cache.focal<=0)cache.morph.active=false;
                        }
                        if(pose_dissolving&&cache.dissolve.active){
                            // Rigid riders only; far specks keep the native swap.
                            if(cache.focal>0&&root.scale>=.2&&build_dissolve(cache.dissolve,cache.root,it->first,false))
                                actor_drawings.back().morph=&cache.dissolve.part;
                        }
                        if(hull_trace&&(pose_morphing||pose_following)&&cache.morph.known&&cache.focal>0&&root.scale>=.5){
                            const auto& m=cache.morph;PoseShape shown=m.to.shape;
                            if(pose_following&&m.active)shown=m.shown;
                            else if(pose_morphing&&m.active&&tick>=m.begin)
                                shown=pose_between(m.start,m.to.shape,pose_curve(std::min(1.,pose_progress(m,tick))));
                            trace_hull(it->first,root.scale,m.to.shape,shown);
                        }
                        const SourcePacket *observed=fresh?&sources->second.back():nullptr;
                        if(traffic_trace)std::fprintf(traffic_trace,"%llu,%d,%06X,%llu,%llu,%d,%.7f,%.7f,%.7f,%.4f,%.4f,%d,%d,%.4f,%.4f,%.4f,%.4f,%u,%llu,%.7f,%.4f,%.4f,%.7f,%.7f,%.4f,%.7f,%.4f,%d,%d,%.5f,%.5f,%.5f,%d,%d,%d,%d,%.5f\n",
                            (unsigned long long)tick,v->scanline,it->first,(unsigned long long)cache.epoch,
                            (unsigned long long)age,held?1:0,cache.root.scale,root.scale,gain,root.x,root.y,
                            cache.canvas.width,cache.canvas.height,
                            root.x+(cache.canvas.x-cache.root.x)*ratio,root.y+(cache.canvas.y-cache.root.y)*ratio,
                            root.x+(cache.canvas.x+cache.canvas.width-cache.root.x)*ratio,
                            root.y+(cache.canvas.y+cache.canvas.height-cache.root.y)*ratio,(unsigned)cache.parts.size(),
                            (unsigned long long)(observed?observed->tick:0),observed?observed->parts[0].scale:cache.root.scale,
                            observed?observed->parts[0].x:cache.root.x,observed?observed->parts[0].y:cache.root.y,
                            observed?observed->world:0,observed?observed->camera:0,cache.focal,display_depth,clip,hill_anchor?1:0,
                            cache.lane_anchor?1:0,cache.curve_center,cache.lane_world,cache.body_offset,cache.raw_height?1:0,far_active?1:0,near_active?1:0,live_gap?1:0,cache.lane_target);
                        // Suppress only SAT entries that still identify this
                        // actor. Header-only matches bridge an interrupted
                        // texture upload with the last complete assembly.
                        for(auto job=pending.rbegin();job!=pending.rend();++job){
                            if(job->projection.actor!=it->first||job->projection.player||tick-job->tick>24||
                               !matches(*job,v,sat,false))continue;
                            bool owned=true;
                            for(unsigned n=0;n<job->pieces.size();++n){unsigned slot=job->first+n;
                                owned&=linked[slot]&&(!used[slot]||actor_slots[slot]==it->first);}
                            if(!owned)continue;
                            for(unsigned n=0;n<job->pieces.size();++n){unsigned slot=job->first+n;
                                bounds[slot].valid=true;replaced[slot]=true;native_slots[slot]=false;
                                used[slot]=true;actor_slots[slot]=it->first;}
                        }
                    }
                }
            }
            ++it;
        }
        std::stable_sort(actor_drawings.begin(),actor_drawings.end(),[](const ActorDrawing&a,const ActorDrawing&b){return a.order<b.order;});
    }
    if(organic){
        // A compacted descriptor slot is not an object's identity. Keep known
        // world objects through the native cull until their full art exits.
        for(auto it=histories.begin();it!=histories.end();){
            auto& h=it->second;
            if(tick-h.seen>60){it=histories.erase(it);continue;}
            if(h.seen!=tick&&tick-h.seen<=40&&h.art.projection.scenery&&h.samples.size()>=2)
                add_billboards(h,true);
            ++it;
        }
        std::stable_sort(billboards.begin(),billboards.end(),[](const Billboard& a,const Billboard& b){return a.z<b.z;});
    }
    if(rival_identity&&!(v->reg[12]&8))identity_build(v);   // D033: before coverage, which needs the rival palette blocks
    if(coverage_enabled)build_coverage();
}

void build_coverage(){
    coverage_sprites.clear();
    for(const auto& drawing:actor_drawings){const auto& cache=*drawing.cache;const auto& art=cache.canvas;double ratio=drawing.ratio;
        size_t first=coverage_sprites.size();
        const ActorArt *part=drawing.morph?drawing.morph:cache.parts.data();
        const size_t part_count=drawing.morph?1:cache.parts.size();
        if(drawing.morph||(arcade_motion()&&!cache.parts.empty()&&(!cache.focal||
           coverage_enabled||(drawing.root.scale>=.25&&art.height*ratio>=16&&art.width*ratio>=8)))){
            for(size_t part_index=0;part_index<part_count;++part_index){const auto& p=part[part_index];
                const auto& image=*p.art;const auto& source=p.source;
                double scale=source.scale*ratio,x=drawing.root.x+(source.x-cache.root.x)*ratio,y=drawing.root.y+(source.y-cache.root.y)*ratio;
                bool hf=(p.attr&0x0800)!=0,vf=(p.attr&0x1000)!=0;
                int ax=hf?image.width-image.anchor_x:image.anchor_x,ay=vf?image.height-image.anchor_y:image.anchor_y;
                const rr::CoverageImage *shown=&image.coverage;unsigned base=p.base;
                auto block=identity_blocks.find(drawing.actor);   // D033: rival helmet/vest colours (line 2 only)
                if(block!=identity_blocks.end()&&p.base==32){
                    shown=&image_roles(image)->coded;base=COVERAGE_BLOCK_BASE+block->second*COVERAGE_BLOCK_SIZE;}
                coverage_sprites.push_back({shown,x-ax*scale,y-ay*scale,scale,drawing.clip,base,drawing.order,hf,vf,(p.attr>>15)!=0,false,false});
            }
        }else{
            const rr::CoverageImage *shown=&art.coverage;unsigned base=0;
            auto block=identity_blocks.find(drawing.actor);
            if(block!=identity_blocks.end()){shown=&canvas_roles(art)->coded;base=COVERAGE_BLOCK_BASE+block->second*COVERAGE_BLOCK_SIZE;}
            coverage_sprites.push_back({shown,drawing.root.x+(art.x-cache.root.x)*ratio,
                drawing.root.y+(art.y-cache.root.y)*ratio,ratio,drawing.clip,base,drawing.order,false,false,true,true,false});
        }
        if(drawing.row_offset!=0||drawing.row_slope!=0)for(size_t n=first;n<coverage_sprites.size();++n){
            coverage_sprites[n].row_offset=drawing.row_offset;coverage_sprites[n].row_slope=drawing.row_slope;
            coverage_sprites[n].row_origin=drawing.root.y;
        }
    }
    for(const auto& b:billboards){
        bool hf=(b.flags&0x8000)!=0,vf=(b.flags&0x4000)!=0;
        int ax=hf?b.art->width-b.art->anchor_x:b.art->anchor_x,ay=vf?b.art->height-b.art->anchor_y:b.art->anchor_y;
        coverage_sprites.push_back({&b.art->coverage,b.pose.x-ax*b.pose.scale,b.pose.y-ay*b.pose.scale,b.pose.scale,b.clip,
            (unsigned)b.palette*16,b.order,hf,vf,b.priority!=0,false,false});
    }
}

void cover(const GVDP *v,const GVDPVisualAuditRow *row){
    coverage_line=-1;
    if(!coverage_enabled||!active||!row_inside(row->line)||!row->background_index||
       !(v->reg[1]&0x40)||(v->reg[12]&8)||(v->reg[12]&6)==6)return;
    std::array<uint32_t,2048> palette;palette.fill(0xff000000);
    for(unsigned i=0;i<192;++i)palette[i]=genesis_dac_cram_to_argb(v->cram[i&63],(int)(i/64));
    palette[192]=0xff000000;const uint32_t *host=gvdp_host_palette();
    for(unsigned i=193;i<256;++i)palette[i]=host?host[i-193]:0xff000000;
    if(!identity_block_looks.empty())identity_blocks_fill(v,palette.data());
    int left=std::max(0,left_edge()+row->offset),right=std::min(row->total,right_edge()+row->offset);
    // Shapes use native coordinates; the backing raster includes wide margins.
    auto sprites=coverage_sprites;for(auto& s:sprites)s.left+=row->offset;
    coverage_mask.fill(0);
    rr::resolve_sprite_row(row->line,left,right,row->background_index,row->background_owner,row->plane_high,
        palette.data(),sprites,coverage_colors.data(),coverage_mask.data());
    coverage_line=row->line;coverage_width=row->total;coverage_frame=tick;
}

void resolve(int line,uint32_t *out,int width){
    if(!coverage_enabled||!active||coverage_line!=line||coverage_frame!=tick||coverage_width!=width)return;
    for(int x=0;x<width&&x<GVDP_MAX_WIDTH;++x)if(coverage_mask[x])out[x]=coverage_colors[x];
}

void audit(void*,const GVDP *v,const GVDPVisualAuditRow *row){
    cover(v,row);
    if(!audit_trace||!active||!row_inside(row->line))return;
    for(int x=view_left;x<view_right;++x){
        unsigned owner=row->native_owner[x];
        if(owner&&owner<=80&&native_slots[owner-1]){
            ++audit_actor_pixels;
            if(!row->owner[x+row->offset]){
                ++audit_actor_missing;
                std::fprintf(audit_trace,"%llu,%d,%d,missing,%u,%u,%u,0\n",
                    (unsigned long long)tick,row->line,x+row->offset,row->native_index[x],
                    row->index[x+row->offset],owner);
            }
        }
        if(!owner||owner>80||!player_slots[owner-1])continue;
        ++audit_player_pixels;
        if(row->native_index[x]!=row->index[x+row->offset]){
            ++audit_player_changes;
            unsigned drawn=row->owner[x+row->offset];
            bool occluded=drawn&&drawn!=owner;
            if(occluded)++audit_player_occluded;else ++audit_player_corrupt;
            std::fprintf(audit_trace,"%llu,%d,%d,%s,%u,%u,%u,%u\n",
                (unsigned long long)tick,row->line,x+row->offset,occluded?"occlusion":"player",row->native_index[x],
                row->index[x+row->offset],owner,row->owner[x+row->offset]);
        }
    }
    if(row->line>=view_horizon+8)for(int x=view_left;x<view_right;++x){
        int px=x+row->offset;
        if(row->native_owner[x]||row->owner[px])continue;
        unsigned old=row->native_index[x],now=row->index[px];if(old>=64||now>=64)continue;
        unsigned c=v->cram[old],r=(c>>1)&7,g=(c>>5)&7,b=(c>>9)&7;
        if(r!=g||r!=b||r<1||r>5)continue;
        c=v->cram[now];r=(c>>1)&7;g=(c>>5)&7;b=(c>>9)&7;
        if(b<4||r>3||g>3||b<=g+1)continue;
        bool object=false;
        for(const auto& bill:billboards){
            int ax=(bill.flags&0x8000)?bill.art->width-bill.art->anchor_x:bill.art->anchor_x;
            int ay=(bill.flags&0x4000)?bill.art->height-bill.art->anchor_y:bill.art->anchor_y;
            double left=bill.pose.x-ax*bill.pose.scale,top=bill.pose.y-ay*bill.pose.scale;
            object|=x+.5>=left&&x+.5<left+bill.art->width*bill.pose.scale&&
                row->line+.5>=top&&row->line+.5<top+bill.art->height*bill.pose.scale;
        }
        if(!object){++audit_blue_road;std::fprintf(audit_trace,"%llu,%d,%d,artifact,%u,%u,0,0\n",
            (unsigned long long)tick,row->line,px,old,now);}
    }
}

void ensure_prepared(const GVDP *v){
    unsigned sat=(v->reg[5]&((v->reg[12]&1)?0x7e:0x7f))<<9;
    if(cached_line!=(int)v->scanline||cached_tick!=tick){
        uint32_t hash=2166136261u;for(unsigned n=0;n<640;++n)hash=hash_byte(hash,v->vram[(sat+n)&65535]);
        if(cached_tick!=tick||hash!=cached_sat){prepare(v,sat);cached_sat=hash;cached_tick=tick;}
        cached_line=v->scanline;
    }
}

int geometry(void*,const GVDP *v,unsigned slot,double *rect){
    if(!enabled||!active||(v->reg[12]&6)==6||slot>=80||!row_inside(v->scanline))return 0;
    ensure_prepared(v);
    unsigned sat=(v->reg[5]&((v->reg[12]&1)?0x7e:0x7f))<<9;
    bool paint_row=v->scanline<view_bottom&&lanes[v->scanline].tick==tick&&lanes[v->scanline].valid;
    if(organic&&lane_model.valid&&markings_ready&&marking_slots[slot]&&(game_index==2||paint_row)){
        native_slots[slot]=false;rect[0]=rect[1]=rect[2]=rect[3]=0;return 1;
    }
    const auto& b=bounds[slot];
    if(!b.valid){
        if(!organic||!live_slots[slot])return 0;
        // Preserve unmatched current animation pieces too. The visual pass
        // must not reintroduce the hardware's 20-sprites-per-line flicker.
        unsigned at=(sat+slot*8)&65535;
        double x=(int)(word(v->vram,at+6)&511)-128;
        double y=(int)(word(v->vram,at)&511)-128;
        unsigned size=v->vram[(at+2)&65535];
        rect[0]=x;rect[1]=y;rect[2]=x+(((size>>2)&3)+1)*8;
        rect[3]=y+((size&3)+1)*8;return 1;
    }
    if(organic&&replaced[slot]){rect[0]=rect[1]=rect[2]=rect[3]=0;return 1;}
    rect[0]=b.x0;rect[1]=b.y0;rect[2]=b.x1;rect[3]=b.y1;
    ++transformed;return 1;
}

void record_road(){
    if(!source_ram||game_index<0)return;
    rr::RoadRays rays;if(!read_road(game_index,source_ram,rays))return;
    if(cameras.empty()||cameras.back().second!=rays.camera){
        cameras.emplace_back(tick,rays.camera);while(cameras.size()>128)cameras.pop_front();
    }
    if(!roads.empty()&&roads.back().rays.camera==rays.camera&&roads.back().rays.y==rays.y&&
       roads.back().rays.x==rays.x&&roads.back().rays.has_x==rays.has_x&&roads.back().rays.has_height==rays.has_height&&
       roads.back().rays.world_y==rays.world_y&&roads.back().rays.horizon==rays.horizon)return;
    // Retained edge objects may outlive 16 road updates. Their last two
    // confirmed camera positions must stay available through that interval.
    roads.push_back({tick,rays});while(roads.size()>128)roads.pop_front();
}

bool surface_depth(double line,double& world,double& depth){
    if(roads.empty())return false;
    const auto& b=roads.back();
    if(!rr::road_depth(b.rays,line,world,depth))return false;
    if(!cameras.empty())world+=camera_at((double)tick)-b.rays.camera;
    return true;
}

int plane_texel(const GVDP *v,unsigned base,int x,int y){
    unsigned at=(base+(((y&255)>>3)*128+((x&1023)>>3))*2)&65535;
    unsigned attr=(v->vram[at]<<8)|v->vram[(at+1)&65535];
    unsigned u=x&7,w=y&7;if(attr&0x0800)u=7-u;if(attr&0x1000)w=7-w;
    unsigned byte=v->vram[((attr&2047)*32+w*4+u/2)&65535];
    return ((attr>>13)&3)*16+((byte>>((u&1)?0:4))&15);
}

int road_color(const GVDP *v,int x,int line,const int scroll[4]){
    auto pixel=[&](unsigned base,int u,int y,int& high){
        unsigned at=(base+(((y&255)>>3)*128+((u&1023)>>3))*2)&65535;
        unsigned attr=word(v->vram,at);high=(attr>>15)&1;
        int color=plane_texel(v,base,u,y);return (color&15)?color:-1;
    };
    int ah,bh;int a=pixel(0x4000,x-scroll[0],line+scroll[2],ah);
    int b=pixel(0x6000,x-scroll[1],line+scroll[3],bh);
    if(a>=0&&ah)return a;if(b>=0&&bh)return b;
    return a>=0?a:(b>=0?b:(v->reg[7]&63));
}

bool road_gray(const GVDP *v,int index){
    if(index<0||index>=64||index==58||index==59)return false;
    unsigned c=v->cram[index],r=(c>>1)&7,g=(c>>5)&7,b=(c>>9)&7;
    return r==g&&r==b&&r>=1&&r<=4;
}
bool road_paint(const GVDP *v,int index){
    if(index<0||index>=64||index==58||index==59)return false;
    unsigned c=v->cram[index],r=(c>>1)&7,g=(c>>5)&7,b=(c>>9)&7;
    return (r>=3&&g>=3&&std::abs((int)r-(int)g)<=1&&b<=2)||(r==g&&r==b&&r>=5);
}

void record_lane(const GVDP *v,int line,const int values[4]){
    if(!row_inside(line)||roads.empty()||v->reg[2]!=0x10||(v->reg[16]&3)!=3)return;
    auto& row=lanes[line];row=LaneRow{};row.tick=tick;row.center=road_centers[line];
    row.camera=roads.back().rays.camera;
    bool depth_valid=rr::road_depth(roads.back().rays,line+.5,row.z,row.depth);
    int center=(int)std::floor(row.center),main=road_color(v,center,line,values);
    if(!depth_valid)return;
    row.valid=road_gray(v,main)||road_paint(v,main);if(!row.valid)return;
    bool asphalt=road_gray(v,main);if(asphalt)row.asphalt=main;
    int left=native_strokes[line].left,right=native_strokes[line].right;
    for(int x=center-8;x<=center+8;++x){
        int color=road_color(v,x,line,values);
        if(road_gray(v,color)){row.asphalt=color;asphalt=true;}
    }
    row.painted=right>=left;row.width=row.painted?right-left+1:0;
    for(int side:{-1,1}){
        int color=road_color(v,center+side*10,line,values);
        if(road_gray(v,color)){row.asphalt=color;asphalt=true;break;}
    }
    row.valid&=asphalt;
}

void fit_lanes(){
    if(lane_fit_epoch==plane_epoch)return;lane_fit_epoch=plane_epoch;
    std::vector<rr::MarkSample> samples;
    for(const auto& row:lanes)if(row.tick==tick&&row.valid)
        samples.push_back({row.z,row.depth,row.width,row.painted});
    // A partial view immediately after unpausing can alias one long gap to
    // another period. Retain only the previously confirmed authored spacing;
    // recalibrate phase/width from current geometry, never a paused image.
    auto fitted=rr::fit_markings(samples,lane_model.valid?lane_model.period:lane_period_hint);
    if(lane_trace){unsigned count=0;for(const auto& s:samples)count+=s.painted;
        std::fprintf(lane_trace,"%llu,%u,%u,%.7f,%.7f,%.7f,%.7f\n",(unsigned long long)tick,
            (unsigned)samples.size(),count,fitted.confidence,fitted.period,fitted.phase,fitted.width_depth);}
    if(!fitted.valid)return;
    if(!lane_model.valid)lane_model=fitted;
    else {
        double delta=std::remainder(fitted.phase-lane_model.phase,lane_model.period);
        lane_model.phase+=std::clamp(delta,-.025,.025);
        lane_model.width_depth=lane_model.width_depth*.9+fitted.width_depth*.1;
        lane_model.confidence=fitted.confidence;
    }
}

bool terrain_art(const GVDP *v){
    if(terrain_ready)return true;
    // The original road bitmap contains a reusable 8px terrain strip. Select
    // a fully covered two-shade tile, avoiding the raster command/HUD rows.
    // Its art and palette remain authored data; only the world UV changes.
    unsigned best=0;
    for(int y0=112;y0<=144;y0+=8){
        std::array<unsigned,64> counts{};std::array<uint8_t,64> pixels;
        for(int y=0;y<8;++y)for(int x=0;x<8;++x){int color=plane_texel(v,0x4000,(view_id?0:480)+x,y0+y);
            pixels[y*8+x]=(uint8_t)color;++counts[color];}
        unsigned unique=0,major=0,base=0;
        for(unsigned i=0;i<64;++i)if(counts[i]){++unique;if(counts[i]>major){major=counts[i];base=i;}}
        if(unique!=2||major<40||major>62||(base&15)==0)continue;
        unsigned score=64-major;if(score<=best)continue;
        best=score;terrain_texture=pixels;
        for(unsigned i=0;i<64;++i)terrain_colors[i]=counts[i]!=0;
    }
    terrain_ready=best!=0;return terrain_ready;
}

int surface(void*,const GVDP *v,int line,int x,int index){
    if(organic&&active&&row_inside(line)&&lane_model.valid&&markings_ready){
        const auto& row=lanes[line];
        if(row.tick==tick&&row.valid&&(road_paint(v,index)||road_gray(v,index))){
            double z=row.z+camera_at((double)tick)-row.camera;
            double half=std::clamp(lane_model.width_depth/row.depth*.5,.45,6.);
            double dx=std::abs(x+.5-row.center);
            if(dx<=8.){
                bool paint=rr::marking_at(lane_model,z)&&dx<half;
                if(paint){++lane_pixels;return row.color;}
                if(road_paint(v,index)){++lane_pixels;return row.asphalt;}
            }
        }
    }
    if(!organic||!active||!row_inside(line)||index<0||index>=64||
       v->reg[2]!=0x10||(v->reg[16]&3)!=3||!terrain_art(v)||!terrain_colors[index])return index;
    auto& row=surface_rows[line];
    if(row.tick!=tick){
        row.tick=tick;double world=0,depth=0,next_world=0,next_depth=0;
        row.valid=surface_depth(line+.5,world,depth);
        if(row.valid){
            // f=128 screen units; one authored 8x8 tile covers .25 world
            // units. Both axes use the SAME inverse-projected road depth.
            row.du=depth*.25/(view_id&&game_index==1?.625:1.);
            row.u=-road_centers[line]*row.du;row.v=world*32.;
            row.footprint=row.du;
            if(surface_depth(std::min(view_bottom-.5,line+1.5),next_world,next_depth))
                row.footprint=std::max(row.footprint,std::abs(next_world-world)*32.);
        }
    }
    if(!row.valid)return index;
    ++terrain_pixels;
    // The game's raster already contains distance-specific grass detail.
    // When world texels become subpixel, retain that authored sample instead
    // of collapsing both shades to solid green. Fade to it over one texel of
    // footprint with a fixed screen-space coverage mask, avoiding UV shimmer
    // and a hard horizontal seam between the two sampling regimes.
    if(row.footprint>=2)return index;
    static constexpr unsigned coverage[]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    if(row.footprint>1&&coverage[(line&3)*4+(x&3)]+.5<(row.footprint-1)*16)return index;
    int u=(int)std::floor(row.u+(x+.5)*row.du),w=(int)std::floor(row.v);
    return terrain_texture[(w&7)*8+(u&7)];
}

int scroll_geometry(void*,const GVDP *v,int line,int *values){
    if(!enabled||!active||!row_inside(line)||(v->reg[12]&6)==6||(v->reg[11]&4))return 0;
    if(organic)ensure_prepared(v);
    if(plane_frame!=tick){
        plane_frame=tick;
        unsigned sat=(v->reg[5]&((v->reg[12]&1)?0x7e:0x7f))<<9;
        uint32_t hash=2166136261u;
        if(!view_id)for(unsigned i=0;i<640;++i)hash=hash_byte(hash,v->vram[(sat+i)&65535]);
        // A road update may leave every SAT rectangle unchanged (e.g. a
        // stationary rider). Include its scroll table so the plane never
        // waits for an unrelated actor to move before accepting new values.
        unsigned hbase=(v->reg[13]&63)<<10;
        unsigned mode=v->reg[11]&3;
        unsigned begin=mode==0?0:view_top*4,bytes=mode==0?4:view_bottom*4;
        for(unsigned i=begin;i<bytes;++i)hash=hash_byte(hash,v->vram[(hbase+i)&65535]);
        for(unsigned i=0;i<2;++i){hash=hash_byte(hash,v->vsram[i]>>8);hash=hash_byte(hash,v->vsram[i]&255);}
        if(!plane_epoch||hash!=plane_hash){
            uint64_t delta=tick-plane_changed;
            plane_duration=plane_epoch&&delta>0&&delta<=12?(double)delta:1.;
            plane_changed=tick;plane_hash=hash;++plane_epoch;
        }
    }
    auto& h=scrolls[line];std::array<int,4> next;
    for(unsigned i=0;i<4;++i)next[i]=values[i]&1023;
    if(!h.valid){h.previous=h.current=next;h.epoch=plane_epoch;h.valid=true;}
    else if(h.epoch!=plane_epoch){
        bool valid=plane_duration>1;
        for(unsigned i=0;i<4;++i){int distance=((next[i]-h.current[i]+512)&1023)-512;if(std::abs(distance)>128)valid=false;}
        h.previous=valid?h.current:next;h.current=next;h.epoch=plane_epoch;
    }
    if(organic){
        if(h.samples.empty()||h.samples.back().first!=plane_changed){
            h.samples.emplace_back(plane_changed,next);while(h.samples.size()>16)h.samples.pop_front();
        }
        // Road pixels contain current-frame raster commands and hill masks.
        // Old vertical scroll or independently delayed half-road edges expose
        // those bytes as blue lines. Preserve V scroll and road width exactly;
        // predict only a common horizontal translation of both halves.
        int translation=0;
        if(h.samples.size()>1){
            const auto& a=h.samples[h.samples.size()-2];const auto& b=h.samples.back();
            double interval=(double)(b.first-a.first);
            if(interval>0&&interval<=12){
                int da=((b.second[0]-a.second[0]+512)&1023)-512;
                int db=((b.second[1]-a.second[1]+512)&1023)-512;
                if(std::abs(da)<64&&std::abs(db)<64)
                    translation=(int)std::lround((da+db)*.5*std::min(2.,(double)(tick-b.first))/interval);
            }
        }
        translation=std::clamp(translation,-4,4);
        values[0]=(values[0]+translation)&1023;values[1]=(values[1]+translation)&1023;
        unsigned table=(v->reg[13]&63)<<10;
        int raw_a=(int16_t)((v->vram[(table+line*4)&65535]<<8)|v->vram[(table+line*4+1)&65535]);
        int raw_b=(int16_t)((v->vram[(table+line*4+2)&65535]<<8)|v->vram[(table+line*4+3)&65535]);
        // Continue either clipped edge from this raster, preserving both V
        // scrolls. Sampling, road center and paint use this same pair.
        values[0]=(values[0]+road_scroll(v,line,0)-raw_a)&1023;
        values[1]=(values[1]+road_scroll(v,line,1)-raw_b)&1023;
        int a_scroll=raw_a+(((values[0]-(raw_a&1023)+512)&1023)-512);
        int b_scroll=raw_b+(((values[1]-(raw_b&1023)+512)&1023)-512);
        road_centers[line]=(a_scroll+b_scroll)*.5;
        record_lane(v,line,values);return 1;
    }
    // All road rows share the same confirmed scene interval. Independent row
    // clocks bend a straight road into horizontal steps at tile boundaries.
    double alpha=std::clamp((tick-plane_changed+1.)/plane_duration,0.,1.);
    for(unsigned i=0;i<4;++i){int delta=((h.current[i]-h.previous[i]+512)&1023)-512;
        values[i]=(int)std::lround(h.previous[i]+alpha*delta)&1023;}
    return 1;
}

void rr_geometry_reset(){
    coverage_line=-1;coverage_frame=UINT64_MAX;coverage_sprites.clear();coverage_mask.fill(0);
    for(const auto& job:pending)if(job.projection.actor&&!job.projection.player){id_archive.push_back(job);}   // D038: keep rival jobs for SAT ownership across the reset
    while(id_archive.size()>160)id_archive.pop_front();
    pending.clear();histories.clear();building_valid=false;cached_tick=UINT64_MAX;cached_line=-1;
    for(auto& item:bounds)item.valid=false;
    for(auto& item:scrolls)item.valid=false;
    plane_frame=UINT64_MAX;plane_hash=0;plane_epoch=0;plane_changed=0;plane_duration=1;
    billboards.clear();replaced.fill(false);live_slots.fill(false);player_slots.fill(false);native_slots.fill(false);projections.clear();
    for(auto& item:scrolls)item.samples.clear();
    roads.clear();cameras.clear();vehicle_cameras.clear();vehicle_camera_frame=UINT64_MAX;
    vehicle_camera_value=vehicle_camera_native=0;terrain_ready=false;terrain_colors.fill(false);
    camera_filter.reset();
    lane_model=rr::MarkingModel{};lane_period_hint=0;lane_fit_epoch=UINT64_MAX;for(auto& row:lanes)row=LaneRow{};
    marking_slots.fill(false);native_strokes.fill(LaneStroke{});markings_ready=false;
    actor_scope=ActorScope{};actor_plans.clear();actor_caches.clear();actor_drawings.clear();actor_sources.clear();early_sources.clear();
    vehicle_pixels.clear();
    for(auto& row:surface_rows)row.tick=UINT64_MAX;
}
void rr_geometry_frame(uint64_t frame,int scene){
    view_margin=wide_scene()?54:0;
    if(!initialized){
        if(const char *path=trace_path("RR_SOURCE_AUDIT"))if((source_audit=std::fopen(path,"wb")))
            std::fputs("frame,actor,epoch,parts,jobs,native_parts,compared,different,accepted,scale,x,y\n",source_audit);
        initialized=true;const char *value=std::getenv("RR_GEOMETRY");enabled=value&&*value=='1';
        value=std::getenv("RR_ORGANIC");organic=enabled&&source_rom&&(!value||*value!='0');
        
        if(const char *path=trace_path("RR_GEOMETRY_TRACE"))if((trace=std::fopen(path,"wb")))
            std::fputs("frame,job,image,native_x,native_y,native_scale,x,y,scale,alpha,actor,component,epoch\n",trace);
        if(organic)if(const char *path=trace_path("RR_BILLBOARD_TRACE"))if((object_trace=std::fopen(path,"wb")))
            std::fputs("frame,key,image,left,top,right,bottom,retained,age,native_x,native_y,native_scale,source_tick,native_depth,size_offset,source_camera,display_camera\n",object_trace);
        if(organic)if(const char *path=trace_path("RR_SCENERY_AUDIT"))if((scenery_trace=std::fopen(path,"wb")))
            std::fputs("frame,serial,image,second,x,y,scale,clip,native_pixels,missing,left,top,right,bottom,flags,attr,pieces,copies,replacement\n",scenery_trace);
        if(organic)if(const char *path=trace_path("RR_TRAFFIC_TRACE"))if((traffic_trace=std::fopen(path,"wb")))
            std::fputs("frame,line,actor,epoch,age,held,native_scale,scale,gain,x,y,width,height,left,top,right,bottom,source_parts,source_tick,source_scale,source_x,source_y,world_z,source_camera,focal,display_depth,hill_clip,hill_anchor,lane_anchor,road_center_x,lane_world,body_offset,world_height,far_hold,near_hold,live_gap,lane_target\n",traffic_trace);
        if(organic)if(const char *path=trace_path("RR_VEHICLE_COVERAGE"))if((pixel_trace=std::fopen(path,"wb")))
            std::fputs("frame,actor,pixels\n",pixel_trace);
        if(organic)if(const char *path=trace_path("RR_LANE_TRACE"))if((lane_trace=std::fopen(path,"wb")))
            std::fputs("frame,samples,painted,confidence,period,phase,width_depth\n",lane_trace);
        if(organic)if(const char *path=trace_path("RR_GEOMETRY_AUDIT"))if((audit_trace=std::fopen(path,"wb")))
            std::fputs("frame,line,x,kind,native,presented,native_owner,owner\n",audit_trace);
        if(enabled)std::fprintf(stderr,"[RR GEOMETRY] object geometry enabled; guest status uses authentic SAT\n");
    }
    // D033: RR1 rival identity (helmet/vest per rider). Experimental switch; RR_RIVAL_IDENTITY=1.
    {const char *value=std::getenv("RR_RIVAL_IDENTITY");
        rival_identity=organic&&(game_index==0||game_index==1||game_index==2)&&!view_id&&value&&value[0]=='1'&&!value[1];}   // D040/D044: RR1 + RR2 + RR3
    // 0.70 Motorcycle Motion: independent of RR_GEOMETRY's own behaviour, but
    // it lives in this presentation layer. Absent/0 = classic 0.60 output.
    // Prototype scope: RR1 rivals in the single forward view only.
    {const char *value=std::getenv("RR_MOTORCYCLE_MOTION");
        pose_easing=organic&&game_index==0&&!view_id&&value&&value[0]=='1'&&!value[1];
        // 2 = prototype 02: reconstructed in-between frames (silhouette morph).
        // 4 = enhanced (the shipped option): prototype 02 for rivals + prototype 03 follower for the player.
        bool hybrid=organic&&game_index==0&&!view_id&&value&&value[0]=='4'&&!value[1];
        pose_morphing=hybrid||(organic&&game_index==0&&!view_id&&value&&value[0]=='2'&&!value[1]);
        if(pose_morphing){const char *frames=std::getenv("RR_MOTORCYCLE_MOTION_FRAMES");
            pose_morph_frames=frames&&*frames?std::clamp(std::atof(frames),4.,24.):10.;}
        // 3 = prototype 03: velocity-continuous hull follower, rivals AND the player.
        pose_following=organic&&game_index==0&&!view_id&&value&&value[0]=='3'&&!value[1];
        if((pose_following||hybrid)&&!pose_easing_logged){
            auto read=[](const char *name,double fallback){const char *text=std::getenv(name);
                return text&&*text?std::clamp(std::atof(text),.25,2.):fallback;};
            pose_follow_rival=read("RR_MOTORCYCLE_MOTION_FOLLOW",.45);
            pose_follow_player=read("RR_MOTORCYCLE_MOTION_FOLLOW_PLAYER",.7);
            const char *player=std::getenv("RR_MOTORCYCLE_MOTION_PLAYER");
            pose_player=!(player&&player[0]=='0');
            // Diagnostic: always draw the player's ROM art through the in-between renderer.
            const char *force=std::getenv("RR_MOTORCYCLE_MOTION_FORCE");
            pose_player_force=pose_player&&force&&force[0]=='1';
        }
        // 6 = prototype 04: temporal pixel dissolve between the two native ROM images
        // (rivals and player). Independent of 1-4; never combined with them.
        pose_dissolving=organic&&game_index==0&&!view_id&&value&&value[0]=='6'&&!value[1];
        if((pose_easing||pose_morphing||pose_following||pose_dissolving)&&!pose_easing_logged){pose_easing_logged=true;
            if(pose_dissolving){
                std::fprintf(stderr,"[RR MOTORCYCLE MOTION] temporal pixel dissolve enabled (RR1 prototype 04, <=3 frames, Bayer 4x4)\n");
                if(const char *path=trace_path("RR_DISSOLVE_TRACE"))if((dissolve_trace=std::fopen(path,"wb")))
                    std::fputs("frame,kind,actor,from_id,to_id,k,frames,threshold,share\n",dissolve_trace);
            }
            else if(hybrid)std::fprintf(stderr,"[RR MOTORCYCLE MOTION] enhanced: rivals prototype 02 + player follower (prototype 03), w=%.2f\n",pose_follow_player);
            else if(pose_following)std::fprintf(stderr,"[RR MOTORCYCLE MOTION] velocity-continuous in-between frames enabled (RR1 prototype 03, rivals%s, w=%.2f/%.2f)\n",
                pose_player?"+player":"",pose_follow_rival,pose_follow_player);
            else std::fprintf(stderr,pose_morphing?"[RR MOTORCYCLE MOTION] rival in-between frames enabled (RR1 prototype 02, %.0f frames)\n":
                "[RR MOTORCYCLE MOTION] enhanced rival pose easing enabled (RR1 prototype)\n",pose_morph_frames);
            if(const char *path=trace_path("RR_POSE_EASE_TRACE"))if((pose_trace=std::fopen(path,"wb")))
                std::fputs(pose_following?"frame,actor,epoch,image,duration,share,row_offset,row_slope,scale,reach_y,box_left,box_top,box_right,box_bottom\n":
                    "frame,actor,epoch,image,duration,share,row_offset,row_slope,scale,reach_y\n",pose_trace);
            if(pose_player){
                if(const char *path=trace_path("RR_POSE_PLAYER_TRACE"))if((player_trace=std::fopen(path,"wb")))
                    std::fputs("frame,actor,epoch,image,duration,share,row_offset,row_slope,scale,reach_y,box_left,box_top,box_right,box_bottom\n",player_trace);
            }
            if(const char *limit=std::getenv("RR_MOTORCYCLE_MOTION_LIMB"))if(*limit)pose_limb_peak=std::clamp(std::atof(limit),4.,999.);
            if(const char *limit=std::getenv("RR_MOTORCYCLE_MOTION_LIMB_RIVAL"))if(*limit)pose_limb_peak_rival=std::clamp(std::atof(limit),4.,999.);
            if(const char *path=trace_path("RR_POSE_COMMIT_TRACE"))if((commit_trace=std::fopen(path,"wb")))
                std::fputs("frame,kind,from_image,to_image,reach,peak,median,kink,limb,dh,from_height,to_height\n",commit_trace);
            if(const char *path=trace_path("RR_POSE_HULL_TRACE"))if((hull_trace=std::fopen(path,"wb")))
                std::fputs("frame,actor,scale,native_height,shown_height,n_l10,n_r10,n_l32,n_r32,n_l54,n_r54,s_l10,s_r10,s_l32,s_r32,s_l54,s_r54\n",hull_trace);
            if(const char *path=trace_path("RR_PLAYER_JOBS_TRACE"))if((player_jobs_trace=std::fopen(path,"wb")))
                std::fputs("frame,address,image,flags,second,x,y,scale,first,pieces,components,component,serial,attr\n",player_jobs_trace);
        }}
    flush_vehicle_pixels();
    if(organic&&active)fit_lanes();
    tick=frame;++frames;
    if(active!=(scene==1)){
        double spacing=lane_model.valid?lane_model.period:lane_period_hint;
        rr_geometry_reset();lane_period_hint=spacing;active=scene==1;
    }
    if(organic&&active)record_road();
    if(organic&&active)for(const auto& item:early_sources){
        const auto& early=item.second;if(frame!=early.tick+1||early.parts.empty())continue;
        auto& packets=actor_sources[item.first];
        // A completed native builder in the same frame takes precedence. If
        // it never ran, the pre-cull projection is still a live world object.
        if(!packets.empty()&&packets.back().tick>=early.tick)continue;
        packets.push_back(early);while(packets.size()>16)packets.pop_front();
        actor_plans[item.first]={early.tick,early.epoch,0,false};
    }
    cached_tick=UINT64_MAX;cached_line=-1;
}
void rr_geometry_job_begin(int game,uint32_t job,const uint8_t *ram){
    building_valid=false;if(!enabled||game<0||game>2||job<0xff0000||job>0xffffe0)return;
    profile=profiles[game];unsigned j=job&65535;
    building=Job{};building.key=job;building.address=job;building.tick=tick;building.first=word(ram,profile.count);
    building.base=longword(ram,profile.sat);building.x=(int16_t)word(ram,j);building.y=(int16_t)word(ram,j+2);
    building.scale=word(ram,j+4)/16384.;building.image=word(ram,j+8);building.flags=word(ram,j+10);
    building.second=word(ram,j+18);
    building.clip=std::min((double)view_bottom,(double)word(ram,j+6));
    constexpr unsigned bases[]={0xd37c,0xceb8,0x36fc},strides[]={20,22,32},depths[]={0x1b50,0x06d6,0x0622};
    if(j>=bases[game]&&(j-bases[game])%strides[game]==0){unsigned n=(j-bases[game])/strides[game];
        if(n<100)building.depth=(int32_t)longword(ram,depths[game]+n*4)/65536.;}
    if(organic){auto meta=projections.find(job);if(meta!=projections.end()&&tick-meta->second.tick<20){
        const auto& p=meta->second;
        // Descriptor slots are reused for actors after scenery compaction.
        // Do not attach stale world identity/art classification to a new job.
        if((!p.scenery&&!p.actor)||(p.image==building.image&&p.second==building.second&&
           p.flags==(building.flags&0xff80)&&p.relative_z==word(ram,j+14))){
            building.projection=p;building.key=p.key;
        }
    }}
    building_valid=building.scale>0&&building.first<80&&building.base>=0xff0000&&building.base<=0xfffd80;
}
void rr_geometry_job_end(const uint8_t *ram){
    if(!building_valid)return;building_valid=false;
    unsigned last=word(ram,profile.count),source=longword(ram,profile.patterns),base=word(ram,profile.tile_base);
    if(last<=(unsigned)building.first||last>80||source<0xff0000||source>0xffff00)return;
    for(unsigned slot=building.first;slot<last;++slot){
        Piece piece;unsigned at=(building.base&65535)+slot*8;
        for(unsigned k=0;k<8;++k)piece.sat[k]=ram[(at+k)&65535];
        unsigned tile=((piece.sat[4]<<8)|piece.sat[5])&2047;
        unsigned cells=(((piece.sat[2]>>2)&3)+1)*((piece.sat[2]&3)+1);
        unsigned delta=(tile-(base&2047))&2047;
        if(delta>1024||(source&65535)+(delta+cells)*32>65536)return;
        piece.pixels=piece_pixels(ram,(source&65535)+delta*32,cells*32);
        building.pieces.push_back(piece);
    }
    building.serial=++serial;pending.push_back(std::move(building));
    while(pending.size()>512)pending.pop_front();
}

void rr_geometry_source(int game,const uint8_t *rom,unsigned bytes,const uint8_t *ram){
    if(game<0||game>2)return;
    configure(game);
    if(game_index!=game||source_rom!=rom){assets.clear();game_index=game;}
    source_rom=rom;source_bytes=bytes;source_ram=ram;
}
void rr_geometry_project(int game,uint32_t job,uint32_t source,uint32_t z,int x,int y){
    if(!organic||game<0||game>2||job<0xff0000||job>0xffffe0)return;
    uint64_t key;
    bool world_source=source<0xff0000;
    if(!world_source)key=0xa000000000000000ull|source;
    else {
        key=14695981039346656037ull;
        uint64_t identity=((uint64_t)z<<32)|((uint64_t)(uint16_t)x<<16)|(uint16_t)y;
        for(int n=0;n<8;++n)key=(key^((identity>>(n*8))&255))*1099511628211ull;
    }
    double camera=source_ram?longword(source_ram,view.camera)/65536.:0;
    // Generic projected objects include debris, riders and traffic. Only the
    // dedicated track-placement hook may authorize full source-art retention.
    projections[job]={key,tick,z/65536.,camera,false};
}
void rr_geometry_actor(int game,uint32_t job,uint32_t actor,const uint8_t *ram){
    if(actor<0xff0000||actor>0xffff80)return;
    if(organic&&active&&game>=0&&game<3&&actor!=view.player&&job>=0xff0000&&job<=0xffffe0){
        unsigned j=job&65535;double scale=word(ram,j+4)/16384.;
        if(scale>0&&scale<=8){
            auto& packet=early_sources[actor];if(packet.parts.empty()||packet.tick!=tick){packet={tick,++actor_epoch,{}};}
            stamp_actor(packet,game,actor,ram);
            if(packet.parts.size()<8)packet.parts.push_back({(double)(int16_t)word(ram,j),(double)(int16_t)word(ram,j+2),scale,
                (double)word(ram,j+6),word(ram,j+8),word(ram,j+18),word(ram,j+10),job});
        }
    }
    if(actor_scope.game==game&&actor_scope.actor==actor&&actor_scope.sources.size()<8&&job>=0xff0000&&job<=0xffffe0){
        unsigned j=job&65535;double scale=word(ram,j+4)/16384.;
        if(scale>0&&scale<=8){actor_scope.sources.push_back({(double)(int16_t)word(ram,j),(double)(int16_t)word(ram,j+2),scale,
            (double)word(ram,j+6),word(ram,j+8),word(ram,j+18),word(ram,j+10),job});++actor_source_captures;}
    }
    rr_geometry_project(game,job,actor,longword(ram,(actor&65535)+4),
        (int16_t)word(ram,(actor&65535)+48),(int16_t)word(ram,(actor&65535)+50));
}
void rr_geometry_actor_begin(int game,uint32_t job,uint32_t actor){
    actor_scope=ActorScope{};
    if(!organic||game<0||game>2||job<0xff0000||job>0xffff80||actor<0xff0000||actor>0xffff80)return;
    actor_scope={game,actor,job};
}
void rr_geometry_actor_end(int game,uint32_t next_job,const uint8_t *ram){
    if(actor_scope.game!=game||!actor_scope.actor)return;
    const auto scope=actor_scope;actor_scope=ActorScope{};
    constexpr unsigned strides[]={20,22,32};
    unsigned stride=strides[game];
    if(next_job<scope.job||next_job>0xffff80||(next_job-scope.job)%stride)return;
    unsigned count=(next_job-scope.job)/stride;if(count>8)return;
    uint64_t epoch=++actor_epoch;bool player=scope.actor==view.player;
    actor_plans[scope.actor]={tick,epoch,count,player};
    if(!player){
        auto early=early_sources.find(scope.actor);
        const auto& precursors=!scope.sources.empty()?scope.sources:
            early!=early_sources.end()&&early->second.tick==tick?early->second.parts:scope.sources;
        std::vector<SourcePart> sources;std::vector<bool> used(precursors.size());
        for(unsigned c=0;c<count;++c){unsigned at=(scope.job+c*stride)&65535;
            double x=(int16_t)word(ram,at);if(game==0&&x<0)x+=512;
            SourcePart part{x,(double)(int16_t)word(ram,at+2),word(ram,at+4)/16384.,(double)word(ram,at+6),
                word(ram,at+8),word(ram,at+18),word(ram,at+10),scope.job+c*stride};
            for(size_t n=0;n<precursors.size();++n){const auto& projected=precursors[n];
                if(!used[n]&&projected.address==part.address&&projected.image==part.image&&projected.second==part.second&&
                   (projected.flags&0xc080)==(part.flags&0xc080)){part=projected;used[n]=true;break;}}
            if(part.scale>0&&part.scale<=8){sources.push_back(part);++actor_source_captures;}
        }
        for(size_t n=0;n<precursors.size();++n)if(!used[n])sources.push_back(precursors[n]);
        auto& packets=actor_sources[scope.actor];packets.push_back({tick,epoch,std::move(sources)});while(packets.size()>16)packets.pop_front();
        stamp_actor(packets.back(),game,scope.actor,ram);
        trace_packet(scope.actor,packets.back(),count,(unsigned)scope.sources.size(),
            early!=early_sources.end()&&early->second.tick==tick?(unsigned)early->second.parts.size():0);
    }
    for(unsigned c=0;c<count;++c){
        uint32_t job=scope.job+c*stride;unsigned j=job&65535;
        Projection p;p.key=0xb000000000000000ull|((uint64_t)(scope.actor&65535)<<8)|c;
        p.tick=tick;p.z=longword(ram,(scope.actor&65535)+4)/65536.;
        p.camera=longword(ram,view.camera)/65536.;
        p.image=word(ram,j+8);p.second=word(ram,j+18);p.flags=word(ram,j+10)&0xff80;p.relative_z=word(ram,j+14);
        p.actor=scope.actor;p.component=c;p.components=count;p.epoch=epoch;p.player=player;
        projections[job]=p;
    }
}
void rr_geometry_scenery(int game,uint32_t job,uint32_t source,unsigned placement,const uint8_t *ram){
    if(!organic||game<0||game>2||job<0xff0000||job>0xffffe0)return;
    unsigned j=job&65535;
    uint32_t camera=longword(ram,view.camera);
    // The road's original projection uses 8.8 relative depth. Recover the
    // fixed track segment, discarding only its intentionally truncated bits.
    uint32_t z=(camera+(int32_t)(int16_t)word(ram,j+14)*256u+32768u)&0xffff0000u;
    uint64_t key=14695981039346656037ull;
    uint64_t identity=((uint64_t)z<<32)|((uint64_t)(source&0xfffff)<<12)|
        ((placement&255)<<2)|((word(ram,j+10)>>14)&3);
    for(int n=0;n<8;++n)key=(key^((identity>>(n*8))&255))*1099511628211ull;
    projections[job]={key,tick,z/65536.,camera/65536.,true,
        word(ram,j+8),word(ram,j+18),word(ram,j+10)&0xff80,word(ram,j+14)};
    auto& p=projections[job];p.has_road=read_road(game,ram,p.road);
    p.size_offset=(placement&0xf0)/32.;
    observe_vehicle_camera(camera/65536.);
}

};
GeometryView views[3];
int layout=0;
int job_view=0;

GeometryView *line_view(int line){
    if(!layout)return &views[0];
    if(line>=8&&line<104)return &views[1];
    if(line>=112&&line<208)return &views[2];
    return nullptr;
}
int dispatch_viewport(void*,const GVDP*,int line,int bounds[2]){
    auto *view=line_view(line);if(!view)return 0;
    bounds[0]=view->left_edge();bounds[1]=view->right_edge();return 1;
}
int dispatch_geometry(void*,const GVDP *v,unsigned slot,double *rect){
    auto *view=line_view(v->scanline);return view?view->geometry(nullptr,v,slot,rect):0;
}
int dispatch_scroll(void*,const GVDP *v,int line,int *values){
    auto *view=line_view(line);return view?view->scroll_geometry(nullptr,v,line,values):0;
}
int dispatch_paused(void*,const GVDP *v,int line,int *values){
    auto *view=line_view(line);return view?view->paused_road_scroll(nullptr,v,line,values):0;
}
void dispatch_objects(void*,const GVDP *v,const GVDPSpriteLayer *layer){
    if(auto *view=line_view(layer->line))view->draw_objects(nullptr,v,layer);
}
int dispatch_surface(void*,const GVDP *v,int line,int x,int index){
    auto *view=line_view(line);
    return view&&x>=view->left_edge()&&x<view->right_edge()?view->surface(nullptr,v,line,x,index):index;
}
void dispatch_audit(void*,const GVDP *v,const GVDPVisualAuditRow *row){
    if(auto *view=line_view(row->line))view->audit(nullptr,v,row);
}
GeometryView& projection_view(int game){
    if(!layout||game<1||game>2)return views[0];
    const uint8_t *ram=views[0].source_ram;
    if(!ram)return views[1];
    unsigned at=game==1?0xd346:0x4032;
    return views[views[0].longword(ram,at)==(game==1?0xff0ee2u:0xff0e32u)?2:1];
}
}
extern "C" void rr_geometry_reset(){for(auto& view:views)view.rr_geometry_reset();}
extern "C" void rr_geometry_enable_coverage(int enabled){
    const char *value=std::getenv("RR_SPRITE_COVERAGE");coverage_enabled=enabled&&(!value||*value!='0');
}
extern "C" void rr_geometry_resolve(const GVDP*,int line,uint32_t *out,int width){
    if(auto *view=line_view(line))view->resolve(line,out,width);
}
extern "C" void rr_geometry_source(int game,const uint8_t *rom,unsigned bytes,const uint8_t *ram){
    for(int i=0;i<3;++i){views[i].view_id=i;views[i].rr_geometry_source(game,rom,bytes,ram);}
}
extern "C" void rr_geometry_frame(uint64_t frame,int scene){
    int next=scene==3||scene==4;
    if(next!=layout){rr_geometry_reset();layout=next;}
    views[0].rr_geometry_frame(frame,layout?0:scene);
    if(layout)for(int i=1;i<3;++i)views[i].rr_geometry_frame(frame,scene==3?1:2);
    auto& view=views[layout?1:0];bool active=view.enabled&&view.active;
    const char *wide=std::getenv("RR_WIDE");
    bool paused=(scene==2||scene==4)&&wide&&*wide=='1';
    gvdp_set_visual_sprites(active?dispatch_geometry:nullptr,nullptr);
    gvdp_set_visual_scroll(active?dispatch_scroll:paused?dispatch_paused:nullptr,nullptr);
    bool identity_hook=!layout&&views[0].enabled&&views[0].organic&&views[0].rival_identity;   // D036
    gvdp_set_visual_objects((active&&view.organic)||identity_hook?dispatch_objects:nullptr,nullptr);
    gvdp_set_visual_surface(active&&view.organic?dispatch_surface:nullptr,nullptr);
    gvdp_set_visual_audit(active&&(view.audit_trace||(coverage_enabled&&view.organic))?dispatch_audit:nullptr,nullptr);
    gvdp_set_visual_viewport(layout?dispatch_viewport:nullptr,nullptr);
}
extern "C" void rr_geometry_job_begin(int game,uint32_t job,const uint8_t *ram){
    job_view=layout?((views[0].word(ram,(job&65535)+10)&0x40)?2:1):0;
    views[job_view].rr_geometry_job_begin(game,job,ram);
}
extern "C" void rr_geometry_job_end(const uint8_t *ram){views[job_view].rr_geometry_job_end(ram);}
extern "C" void rr_geometry_project(int game,uint32_t job,uint32_t source,uint32_t z,int x,int y){
    projection_view(game).rr_geometry_project(game,job,source,z,x,y);
}
extern "C" void rr_geometry_actor(int game,uint32_t job,uint32_t actor,const uint8_t *ram){
    projection_view(game).rr_geometry_actor(game,job,actor,ram);
}
extern "C" void rr_geometry_actor_begin(int game,uint32_t job,uint32_t actor){
    projection_view(game).rr_geometry_actor_begin(game,job,actor);
}
extern "C" void rr_geometry_actor_end(int game,uint32_t next_job,const uint8_t *ram){
    projection_view(game).rr_geometry_actor_end(game,next_job,ram);
}
extern "C" void rr_geometry_scenery(int game,uint32_t job,uint32_t source,unsigned placement,const uint8_t *ram){
    projection_view(game).rr_geometry_scenery(game,job,source,placement,ram);
}
