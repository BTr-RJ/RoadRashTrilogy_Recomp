#include "road_surface.h"
#include <cmath>
#include <algorithm>
#include <vector>
namespace rr {
RoadRays road_camera_basis(const RoadRays& current,const RoadRays& previous,double camera){
    auto result=current;double dz=current.camera-previous.camera;
    if(dz<=1e-6||dz>2||!current.has_height||!previous.has_height)return result;
    int shift=(int)std::floor(current.camera)-(int)std::floor(previous.camera);
    std::vector<double> deltas;
    for(int i=4;i<24;++i)if(i+shift>=0&&i+shift<32)
        deltas.push_back(current.world_y[i]-previous.world_y[i+shift]);
    if(deltas.empty())return result;
    std::sort(deltas.begin(),deltas.end());double delta=deltas[deltas.size()/2];
    std::vector<double> errors;for(double d:deltas)errors.push_back(std::abs(d-delta));
    std::sort(errors.begin(),errors.end());
    double factor=std::clamp((camera-current.camera)/dz,-.5,1.);
    if(errors[errors.size()/2]<=1.)for(auto& y:result.world_y)y+=delta*factor;
    if(current.has_x&&previous.has_x){
        double n=0,sx=0,sy=0,sxx=0,sxy=0;
        for(int i=4;i<24;++i)if(i+shift>=0&&i+shift<32){
            double d=current.x[i]-previous.x[i+shift];
            ++n;sx+=i;sy+=d;sxx+=i*i;sxy+=i*d;
        }
        double denominator=n*sxx-sx*sx;
        if(denominator>0){
            double slope=(n*sxy-sx*sy)/denominator,intercept=(sy-slope*sx)/n;
            errors.clear();
            for(int i=4;i<24;++i)if(i+shift>=0&&i+shift<32)
                errors.push_back(std::abs(current.x[i]-previous.x[i+shift]-(intercept+slope*i)));
            std::sort(errors.begin(),errors.end());
            if(errors[errors.size()/2]<=2.)for(int i=0;i<32;++i)result.x[i]+=(intercept+slope*i)*factor;
        }
    }
    return result;
}
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
            depth=rays.position_bias+1./((1-alpha)/(zn-rays.position_bias)+alpha/(zf-rays.position_bias));
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
    double alpha=(1./road_position_depth(rays,depth)-1./(zn-rays.position_bias))/(1./(zf-rays.position_bias)-1./(zn-rays.position_bias));
    screen_y=rays.y[near]+alpha*(rays.y[near+1]-rays.y[near]);
    clip_y=rays.clip;
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
    screen_y=rays.horizon+(y-height)*rays.height_scale/road_position_depth(rays,depth);return std::isfinite(screen_y);
}
}
