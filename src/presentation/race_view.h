#pragma once

namespace rr {
// Native split-screen view rectangles. Coordinates remain in the original
// 320x224 raster; side HUDs and the divider are never part of a road view.
struct RaceView {
    int id=0,left=0,top=0,right=320,bottom=152;
    double center=160,horizon=64,focal=1;
    unsigned camera=0,road_y=0,road_xy=0,player=0;
    bool contains_row(int y) const {return y>=top&&y<bottom;}
};
inline RaceView race_view(int game,int player){
    RaceView v;
    constexpr unsigned players[]={0xff1eaa,0xff0a62,0xff09b2};
    constexpr unsigned cameras[]={0x1e68,0x0a0a,0x0956};
    if(game<0||game>2)return v;
    v.player=players[game];v.camera=cameras[game];
    if(game==0||player<1||player>2)return v;
    v.id=player;v.focal=.75;
    if(player==1){v.left=0;v.right=208;v.top=8;v.bottom=104;v.center=104;v.horizon=56;}
    else {v.left=112;v.right=320;v.top=112;v.bottom=208;v.center=216;v.horizon=160;v.player+=0x480;}
    if(game==1){v.camera=player==1?0x09f2:0x09fe;v.road_y=player==1?0xd2b8:0xd2fc;v.road_xy=player==1?0x1cf4:0x1fb0;}
    else {v.camera=player==1?0x093e:0x094a;v.road_y=player==1?0x3f1c:0x3fa4;v.road_xy=player==1?0x1cee:0x1faa;}
    return v;
}
}
