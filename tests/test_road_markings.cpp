#include "road_markings.h"
#include <cmath>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"markings line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    rr::MarkingModel authored;authored.period=2;authored.phase=.375;authored.duty=.5;
    std::vector<rr::MarkSample> samples;
    for(int i=0;i<100;++i){double z=i*.137,depth=2+i*.02;samples.push_back({z,depth,4./depth,rr::marking_at(authored,z)});}
    auto m=rr::fit_markings(samples);CHECK(m.valid&&m.period==2&&m.confidence==1);
    CHECK(std::abs(m.phase-.375)<1e-9&&std::abs(m.width_depth-4)<1e-9);
    CHECK(rr::marking_at(m,0)&&!rr::marking_at(m,.7));
    CHECK(rr::marking_at(m,-.4)==rr::marking_at(m,1.6));
    for(auto& s:samples)s.painted=true;CHECK(!rr::fit_markings(samples).valid);
    for(auto& s:samples)s.painted=false;CHECK(!rr::fit_markings(samples).valid);
    // A partial dash after resume must not recalibrate known spacing to a
    // long-period alias. Keep native paint until that spacing is observable.
    samples.clear();
    for(int i=0;i<40;++i)samples.push_back({i*.1,2,2,i>=20});
    CHECK(rr::fit_markings(samples).valid);
    CHECK(!rr::fit_markings(samples,2).valid);
    std::puts("Road markings: authored rhythm/width, perspective phase and solid-road fallback passed");
}
