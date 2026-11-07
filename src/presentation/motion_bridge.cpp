#include "motion_bridge.h"
#include <cstdlib>
#include <cstdio>
// Image-flow warping cannot reconstruct sprite occlusion or the software road.
// Ignore legacy RR_SMOOTH=1 as well: only complete guest pictures are displayed.
extern "C" void rr_motion_scene(int,uint64_t){}
extern "C" const uint32_t *rr_motion_present(const uint32_t *input,int stride,int width,int height,uint64_t frame) {
    static bool warned;
    if(!warned) {
        warned=true;
        const char *on=std::getenv("RR_SMOOTH");
        if(on&&*on=='1')std::fprintf(stderr,"[RR VIDEO] Image motion warping removed; presenting complete original frames.\n");
    }
    return input;
}
