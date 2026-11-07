#pragma once
#include <array>
namespace rr {
struct RoadRays {
    double camera=0;
    std::array<double,32> y{};
    std::array<double,32> x{};
    std::array<double,32> world_y{};
    double horizon=76;
    bool has_x=false;
    bool has_height=false; // populated native Y observations, separate from X availability
};
// Invert the game's projected, hill-clipped Z map. Interpolation in 1/Z
// preserves perspective; interpolation in Z would accelerate each strip.
bool road_depth(const RoadRays& rays,double screen_y,double& world_z,double& depth);
// Forward projection against the current hill profile. The occluding crest
// stays in screen space; it must not be enlarged around a sprite's anchor.
bool road_point(const RoadRays& rays,double depth,double& screen_y,double& clip_y);
bool road_lateral(const RoadRays& rays,double depth,double& world_x);
bool road_vehicle_height(const RoadRays& rays,double depth,double height,double& screen_y);
// Continue a raster edge that hit a native bitmap clamp, using its last
// confirmed slope in this frame. Unclipped rows and discontinuities fall back.
int extend_clipped_edge(const std::array<int,152>& edge,unsigned line,int limit,bool maximum=false);
}
