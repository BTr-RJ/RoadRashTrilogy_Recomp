#pragma once
#include <vector>
namespace rr {
struct MarkSample {double z=0,depth=1,width=0;bool painted=false;};
struct MarkingModel {double period=2,phase=0,duty=.5,width_depth=4,confidence=0;bool valid=false;};
MarkingModel fit_markings(const std::vector<MarkSample>& samples,double period=0);
bool marking_at(const MarkingModel& model,double world_z);
}
