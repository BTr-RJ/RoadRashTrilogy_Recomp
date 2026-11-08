#pragma once
#include <cstdint>
#include <vector>
namespace rr {
// Exact area coverage of the current source texels. No past frame participates.
class CoverageImage {
    struct Bin {uint8_t color,high;std::vector<uint32_t> integral;};
    mutable std::vector<Bin> bins;
    void prepare() const;
public:
    int width=0,height=0;
    std::vector<uint8_t> index,opaque,high;
    void build(int w,int h,const uint8_t *pixels,const uint8_t *mask=nullptr,const uint8_t *priority=nullptr);
    struct Sample {double r=0,g=0,b=0,a=0;};
    Sample area(double x0,double y0,double x1,double y1,double pixel_area,
                const uint32_t *palette,unsigned base,bool high_only) const;
};
struct CoverageSprite {
    const CoverageImage *image=nullptr;
    double left=0,top=0,scale=1,clip=0;
    unsigned base=0,order=81;
    bool flip_x=false,flip_y=false,priority=true,per_pixel_priority=false,crisp=false;
};
void resolve_sprite_row(int line,int left,int right,const uint8_t *background,
    const uint8_t *owner,const uint8_t *plane_high,const uint32_t *palette,
    const std::vector<CoverageSprite>& sprites,uint32_t *colors,uint8_t *mask);
}
