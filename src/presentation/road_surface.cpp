#include "road_surface.h"
#include <cmath>
#include <algorithm>
namespace rr {
int extend_clipped_edge(const std::array<int,152>& edge,unsigned line,int limit,bool maximum){
    if(maximum){
        std::array<int,152> mirrored;
        for(unsigned i=0;i<edge.size();++i)mirrored[i]=-edge[i];
        return -extend_clipped_edge(mirrored,line,-limit);
    }
    if(line>=edge.size()||line<8||edge[line]!=limit)return line<edge.size()?edge[line]:limit;
    unsigned anchor=line;
    while(anchor>8&&edge[anchor]==limit)--anchor;
    if(edge[anchor]<=limit||edge[anchor]>limit+32)return limit;
    unsigned start=anchor-8;
    for(unsigned i=start+1;i<=anchor;++i){
        int delta=edge[i]-edge[i-1];
        if(edge[i]<=limit||delta>0||delta<-32)return limit;
    }
    double slope=(edge[anchor]-edge[start])/8.;
    if(slope>=-.25)return limit;
    // A half-turn in a 10-bit fetch has two signed representatives. Stay
    // strictly below 512 so a positive continuation cannot wrap to the left.
    return std::clamp((int)std::lround(edge[anchor]+slope*(line-anchor)),limit-511,limit);
}
bool road_depth(const RoadRays& rays,double screen_y,double& world_z,double& depth){
    const double fraction=rays.camera-std::floor(rays.camera);
    unsigned near=0;
    for(unsigned far=1;far<32;++far){
        if(rays.y[far]==rays.y[near]){near=far;continue;}
        if(rays.y[far]>rays.y[near])continue; // hidden behind a nearer crest
        if(screen_y<=rays.y[near]&&screen_y>=rays.y[far]){
            double alpha=(rays.y[near]-screen_y)/(rays.y[near]-rays.y[far]);
            double zn=2.+near-fraction,zf=2.+far-fraction;
            depth=1./((1-alpha)/zn+alpha/zf);
            world_z=rays.camera+depth-2.;return true;
        }
        near=far;
    }
    return false;
}
bool road_point(const RoadRays& rays,double depth,double& screen_y,double& clip_y){
    double fraction=rays.camera-std::floor(rays.camera),q=depth-2.+fraction;
    if(!std::isfinite(depth)||depth<.25||q>33.)return false;
    int near=std::clamp((int)std::floor(q),0,30);
    double zn=2.+near-fraction,zf=zn+1;
    double alpha=(1./depth-1./zn)/(1./zf-1./zn);
    screen_y=rays.y[near]+alpha*(rays.y[near+1]-rays.y[near]);
    clip_y=152.;
    for(int i=0;i<=near&&i<=q;++i)clip_y=std::min(clip_y,rays.y[i]);
    return std::isfinite(screen_y)&&std::isfinite(clip_y);
}
bool road_lateral(const RoadRays& rays,double depth,double& world_x){
    double q=depth-2.+rays.camera-std::floor(rays.camera);
    if(!rays.has_x||!std::isfinite(q)||depth<.25||q>33.)return false;
    int near=std::clamp((int)std::floor(q),0,30);
    world_x=rays.x[near]+(q-near)*(rays.x[near+1]-rays.x[near]);
    return std::isfinite(world_x);
}
bool road_vehicle_height(const RoadRays& rays,double depth,double height,double& screen_y){
    double q=depth-2.+rays.camera-std::floor(rays.camera);
    if(!rays.has_x||!std::isfinite(q)||depth<.25||q>33.)return false;
    int near=std::clamp((int)std::floor(q),0,30);
    double y=rays.world_y[near]+(q-near)*(rays.world_y[near+1]-rays.world_y[near]);
    screen_y=rays.horizon+(y-height)/depth;return std::isfinite(screen_y);
}
}
