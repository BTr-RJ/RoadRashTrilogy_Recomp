#include "road_markings.h"
#include <algorithm>
#include <cmath>
namespace rr {
bool marking_at(const MarkingModel& m,double z){
    double phase=std::fmod(z+m.phase,m.period);if(phase<0)phase+=m.period;
    return phase<m.period*m.duty;
}
MarkingModel fit_markings(const std::vector<MarkSample>& samples,double period){
    MarkingModel best;if(samples.size()<20)return best;
    unsigned painted=0;std::vector<double> widths;
    for(const auto& s:samples){painted+=s.painted;if(s.painted&&s.width>=2)widths.push_back(s.width*s.depth);}
    if(painted<4||painted+4>samples.size())return best; // preserve solid/unmarked roads
    const double periods[]={.5,1,2,4,8};const double duties[]={.4,.5,.6};
    unsigned best_score=0;
    for(double p:periods){
        if(period&&p!=period)continue;
        for(double duty:duties)for(unsigned step=0;step<64;++step){
            MarkingModel m;m.period=p;m.phase=p*step/64.;m.duty=duty;
            unsigned score=0;for(const auto& s:samples)score+=marking_at(m,s.z)==s.painted;
            if(score>best_score){best_score=score;best=m;}
        }
    }
    best.confidence=(double)best_score/samples.size();best.valid=best.confidence>=.80;
    if(!widths.empty()){
        std::sort(widths.begin(),widths.end());best.width_depth=std::clamp(widths[widths.size()/2],1.,24.);
    }
    return best;
}
}
