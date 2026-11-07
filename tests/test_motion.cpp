#include "presentation/motion_bridge.h"
#include <vector>
#include <cstdlib>
#include <cstdio>
int main() {
    // Existing user profiles request RR_SMOOTH=1. No scene or repeated frame
    // may reactivate image warping, create ghost copies or add frame latency.
    _putenv_s("RR_SMOOTH","1");
    for(int width:{256,320,426,428}) {
        const int stride=512,height=224;
        std::vector<uint32_t> image(stride*height);
        for(uint64_t frame=0;frame<30;++frame) {
            for(size_t i=0;i<image.size();++i)image[i]=0xff000000u|(uint32_t)((i*37+frame*512)&0xffffff);
            auto original=image;
            rr_motion_scene(frame%3!=0,frame/6);
            const auto *output=rr_motion_present(image.data(),stride,width,height,frame);
            if(output!=image.data() || image!=original) {
                std::fprintf(stderr,"FAIL: original image altered/delayed at width=%d frame=%llu\n",width,(unsigned long long)frame);return 1;
            }
        }
    }
    std::puts("Motion regression: legacy setting, cuts, repeated pictures and H32/H40/wide are exact passthrough");
    return 0;
}
