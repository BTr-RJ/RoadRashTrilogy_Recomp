#include "sprite_coverage.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace rr {
void CoverageImage::build(int w,int h,const uint8_t *pixels,const uint8_t *mask,const uint8_t *priority){
    width=w;height=h;bins.clear();index.assign(pixels,pixels+(size_t)w*h);
    opaque.resize(index.size());high.resize(index.size());
    for(size_t i=0;i<index.size();++i){opaque[i]=mask?mask[i]:index[i]!=0;high[i]=priority?priority[i]:1;}
}
void CoverageImage::prepare() const {
    if(!bins.empty()||index.empty())return;
    std::array<int,512> lookup;lookup.fill(-1);
    for(size_t i=0;i<index.size();++i)if(opaque[i]){
        unsigned key=index[i]+256*(high[i]!=0);
        if(lookup[key]<0){lookup[key]=(int)bins.size();bins.push_back({index[i],(uint8_t)(high[i]!=0),{}});}
    }
    for(auto& bin:bins){
        bin.integral.assign((size_t)(width+1)*(height+1),0);
        for(int y=0;y<height;++y){uint32_t sum=0;
            for(int x=0;x<width;++x){size_t i=(size_t)y*width+x;
                sum+=opaque[i]&&index[i]==bin.color&&(high[i]!=0)==(bin.high!=0);
                bin.integral[(size_t)(y+1)*(width+1)+x+1]=sum+bin.integral[(size_t)y*(width+1)+x+1];
            }
        }
    }
}
CoverageImage::Sample CoverageImage::area(double x0,double y0,double x1,double y1,double pixel_area,
                                         const uint32_t *palette,unsigned base,bool high_only) const {
    Sample result;
    x0=std::clamp(x0,0.,(double)width);x1=std::clamp(x1,0.,(double)width);
    y0=std::clamp(y0,0.,(double)height);y1=std::clamp(y1,0.,(double)height);
    if(x1<=x0||y1<=y0||pixel_area<=0)return result;
    auto add=[&](unsigned color,double area){
        uint32_t p=palette[(base+color)&255];double a=area/pixel_area;
        result.r+=((p>>16)&255)*a;result.g+=((p>>8)&255)*a;result.b+=(p&255)*a;result.a+=a;
    };
    int xa=(int)std::floor(x0),xb=(int)std::ceil(x1),ya=(int)std::floor(y0),yb=(int)std::ceil(y1);
    if((xb-xa)*(yb-ya)<=25){
        for(int y=ya;y<yb;++y)for(int x=xa;x<xb;++x){size_t i=(size_t)y*width+x;
            if(!opaque[i]||(high_only&&!high[i]))continue;
            double a=(std::min(x1,x+1.)-std::max(x0,(double)x))*(std::min(y1,y+1.)-std::max(y0,(double)y));
            add(index[i],a);
        }
    }else{
        prepare();
        auto cumulative=[&](const Bin& bin,double x,double y){
            int ix=std::min((int)x,width-1),iy=std::min((int)y,height-1);double fx=x-ix,fy=y-iy;
            size_t p=(size_t)iy*(width+1)+ix;const auto& t=bin.integral;
            return (1-fy)*((1-fx)*t[p]+fx*t[p+1])+fy*((1-fx)*t[p+width+1]+fx*t[p+width+2]);
        };
        for(const auto& bin:bins)if(!high_only||bin.high){
            double a=cumulative(bin,x1,y1)-cumulative(bin,x0,y1)-cumulative(bin,x1,y0)+cumulative(bin,x0,y0);
            if(a>0)add(bin.color,a);
        }
    }
    result.a=std::clamp(result.a,0.,1.);return result;
}
void resolve_sprite_row(int line,int left,int right,const uint8_t *background,const uint8_t *owner,
    const uint8_t *plane_high,const uint32_t *palette,const std::vector<CoverageSprite>& sprites,uint32_t *colors,uint8_t *mask){
    std::vector<std::array<double,3>> rgb((size_t)right);
    for(int x=left;x<right;++x){uint32_t c=palette[background[x]];rgb[x]={(double)((c>>16)&255),(double)((c>>8)&255),(double)(c&255)};mask[x]=0;}
    std::vector<const CoverageSprite*> order;order.reserve(sprites.size());
    for(const auto& sprite:sprites)order.push_back(&sprite);
    std::stable_sort(order.begin(),order.end(),[](auto a,auto b){return a->order<b->order;});
    for(auto it=order.rbegin();it!=order.rend();++it){const auto& s=**it;
        if(!s.image||s.scale<=0||line+1<=s.top||line>=std::min(s.clip,s.top+s.image->height*s.scale))continue;
        int x0=std::max(left,(int)std::floor(s.left)),x1=std::min(right,(int)std::ceil(s.left+s.image->width*s.scale));
        double v0=(line-s.top)/s.scale,v1=(std::min(line+1.,s.clip)-s.top)/s.scale;
        if(s.flip_y){double t=s.image->height-v1;v1=s.image->height-v0;v0=t;}
        for(int x=x0;x<x1;++x){
            if(owner[x]&&owner[x]<=s.order)continue;
            if(plane_high[x]&&!s.priority&&!s.per_pixel_priority)continue;
            double u0=(x-s.left)/s.scale,u1=(x+1.-s.left)/s.scale;
            if(s.flip_x){double t=s.image->width-u1;u1=s.image->width-u0;u0=t;}
            auto c=s.image->area(u0,v0,u1,v1,1./(s.scale*s.scale),palette,s.base,plane_high[x]&&s.per_pixel_priority);
            if(c.a<=0)continue;
            if(s.crisp&&c.a>1.-1e-9){
                int u=(int)std::floor((x+.5-s.left)/s.scale),v=(int)std::floor((line+.5-s.top)/s.scale);
                if(s.flip_x)u=s.image->width-1-u;if(s.flip_y)v=s.image->height-1-v;
                if(u>=0&&u<s.image->width&&v>=0&&v<s.image->height){size_t q=(size_t)v*s.image->width+u;
                    if(s.image->opaque[q]){auto p=palette[(s.base+s.image->index[q])&255];c.r=(p>>16)&255;c.g=(p>>8)&255;c.b=p&255;}
                }
            }
            rgb[x][0]=c.r+(1-c.a)*rgb[x][0];rgb[x][1]=c.g+(1-c.a)*rgb[x][1];rgb[x][2]=c.b+(1-c.a)*rgb[x][2];mask[x]=1;
        }
    }
    for(int x=left;x<right;++x)if(mask[x]){
        auto channel=[](double v){return (uint32_t)std::clamp((int)std::lround(v),0,255);};
        colors[x]=0xff000000u|(channel(rgb[x][0])<<16)|(channel(rgb[x][1])<<8)|channel(rgb[x][2]);
    }
}
}
