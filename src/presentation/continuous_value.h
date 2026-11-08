#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rr {
// A native integer anchor describes an interval, not a noisy new world point.
// Retain the compatible world point; a genuine change is followed immediately.
struct QuantizedAnchor {
    double value=0;uint64_t frame=UINT64_MAX;
    void reset(){frame=UINT64_MAX;}
    double step(uint64_t now,double target,double radius){
        if(frame==UINT64_MAX||now<frame||now-frame>12)value=target;
        else if(frame!=now)value=std::clamp(value,target-radius,target+radius);
        frame=now;return value;
    }
};
// A causal critically damped coordinate correction, never an image buffer.
// Native artwork/input stays current. Repeated queries in one raster frame
// share one value, even when a later upload brings a new observation.
struct ContinuousValue {
    double value=0,velocity=0;
    uint64_t frame=UINT64_MAX;
    void reset(){frame=UINT64_MAX;velocity=0;}
    double track(uint64_t now,double target,double target_velocity,double separation,double omega=.6){
        if(frame==now)return value;
        double dt=frame==UINT64_MAX||now<frame?0:double(now-frame);
        if(dt<=0||dt>12){value=target;velocity=target_velocity;}
        else {
            double error=value-(target-target_velocity*dt);
            double j=(velocity-target_velocity+omega*error)*dt,decay=std::exp(-omega*dt);
            value=target+(error+j)*decay;
            velocity=target_velocity+(velocity-target_velocity-omega*j)*decay;
            value=std::clamp(value,target-separation,target+separation);
        }
        frame=now;return value;
    }
    double step(uint64_t now,double target,double separation){
        if(frame==now)return value;
        double dt=frame==UINT64_MAX||now<frame?0:double(now-frame);
        if(dt<=0||dt>12){value=target;velocity=0;}
        else {
            constexpr double omega=1.0;
            double error=value-target,j=(velocity+omega*error)*dt,decay=std::exp(-omega*dt);
            value=target+(error+j)*decay;
            velocity=(velocity-omega*j)*decay;
            value=std::clamp(value,target-separation,target+separation);
        }
        frame=now;return value;
    }
};
}
