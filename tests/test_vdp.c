#include "genesis_vdp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
uint32_t g_audio_cycle_counter;
static uint32_t phase;
uint32_t rr_raster_phase(void){return phase;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"VDP FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static GVDP vdp;
static void word(unsigned addr,unsigned value){vdp.vram[addr]=(uint8_t)(value>>8);vdp.vram[addr+1]=(uint8_t)value;}
static int transform_mode;
static int audit_correct;
static void audit(void *user,const GVDP *v,const GVDPVisualAuditRow *row){
    (void)user;(void)v;
    if(row->line==40)audit_correct=row->native_index[24]==1&&row->native_owner[24]==1&&
        row->index[24]==0&&row->index[61]==1&&row->owner[61]==1;
}
static int identity(void *user,const GVDP *v,unsigned slot,double bounds[4]){
    (void)user;unsigned at=0xc000+slot*8;
    bounds[0]=(((v->vram[at+6]<<8)|v->vram[at+7])&511)-128;
    bounds[1]=(((v->vram[at]<<8)|v->vram[at+1])&511)-128;
    bounds[2]=bounds[0]+(((v->vram[at+2]>>2)&3)+1)*8;
    bounds[3]=bounds[1]+((v->vram[at+2]&3)+1)*8;return 1;
}
static int surface_min=9999,surface_max=-9999;
static int passthrough(void *user,const GVDP *v,int line,int x,int index){
    (void)user;(void)v;(void)line;(void)x;return index;
}
static int road_shift(void *user,const GVDP *v,int line,int values[4]){
    (void)user;(void)v;if(line!=144)return 0;
    values[0]=(values[0]-4)&1023;values[1]=(values[1]-4)&1023;return 1;
}
static int surface(void *user,const GVDP *v,int line,int x,int index){
    (void)user;(void)v;(void)line;(void)index;
    if(x<surface_min)surface_min=x;if(x>surface_max)surface_max=x;return 7;
}
static int visual_rect(void *user,const GVDP *v,unsigned slot,double bounds[4]){
    (void)user;(void)v;if(slot>1)return 0;
    bounds[0]=transform_mode?60.5+slot*20:24+slot*20;
    bounds[1]=40;bounds[2]=bounds[0]+12;bounds[3]=52;
    return 1;
}
int main(void){
    memset(&vdp,0,sizeof vdp);vdp.reg[1]=0x40;vdp.reg[12]=1;
    vdp.reg[2]=0x30;vdp.reg[4]=7;vdp.reg[5]=0x61; // H40 ignores SAT register bit 0
    memset(vdp.vram+32,0x11,32); // opaque 8x8 tile 1
    word(0xc000,128+40+0x200); // unused sprite Y bit 9 must be ignored
    word(0xc002,0);word(0xc004,0x8001);word(0xc006,128+24+0x200);
    uint8_t row[GVDP_MAX_WIDTH];
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);
    for(int x=24;x<32;++x)CHECK(row[x]==1);
    CHECK(row[23]==0&&row[32]==0);
    gvdp_set_ws_extra(54);CHECK(gvdp_render_scanline(&vdp,40,row)==428);
    for(int x=78;x<86;++x)CHECK(row[x]==1); // expanded canvas retains exact sprite scale
    gvdp_set_ws_extra(0);
    // Continuous sampling must retain transparency and leave native sprite
    // collision/overflow untouched, including collisions hidden visually.
    memset(&vdp,0,sizeof vdp);vdp.reg[1]=0x40;vdp.reg[12]=1;
    vdp.reg[2]=0x30;vdp.reg[4]=7;vdp.reg[5]=0x60;
    memset(vdp.vram+32,0x11,32);memset(vdp.vram+32+4*3,0,4);
    word(0xc000,168);word(0xc002,1);word(0xc004,0x8001);word(0xc006,152);
    word(0xc008,168);word(0xc00a,0);word(0xc00c,0x8001);word(0xc00e,152);
    transform_mode=1;gvdp_set_visual_sprites(visual_rect,NULL);
    gvdp_set_visual_audit(audit,NULL);
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);
    CHECK(audit_correct);gvdp_set_visual_audit(NULL,NULL);
    CHECK(vdp.sprite_collision); // original overlapping SAT still collides
    CHECK(row[24]==0&&row[61]==1&&row[79]==0&&row[81]==1);
    CHECK(gvdp_render_scanline(&vdp,44,row)==320); // transparent source row 3
    CHECK(row[61]==0&&row[81]==0);
    word(0xc00e,172); // original SAT now separates the actors
    vdp.sprite_collision=vdp.sprite_overflow=0;
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);
    CHECK(!vdp.sprite_collision&&!vdp.sprite_overflow);
    CHECK(vdp.vram[0xc007]==152&&vdp.vram[0xc00f]==172);
    gvdp_set_visual_sprites(NULL,NULL);
    // A material-only pass must run, preserve visible sprites and retain the
    // collision flags evaluated from the unmodified native SAT.
    gvdp_set_visual_surface(surface,NULL);
    vdp.sprite_collision=vdp.sprite_overflow=0;
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);
    CHECK(row[0]==7&&row[24]==1&&row[44]==1&&row[80]==7);
    CHECK(surface_min==0&&surface_max==319);
    CHECK(!vdp.sprite_collision&&!vdp.sprite_overflow);
    word(0xc00e,152);
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);CHECK(vdp.sprite_collision);
    gvdp_set_ws_extra(54);surface_min=9999;surface_max=-9999;
    CHECK(gvdp_render_scanline(&vdp,40,row)==428);
    CHECK(surface_min==-54&&surface_max==373); // world UV retains the native origin
    CHECK(row[78]==1&&row[54]==7&&vdp.sprite_collision);
    gvdp_set_ws_extra(0);
    gvdp_set_visual_surface(NULL,NULL);
    // Dense approaching riders must stay visible in the presentation pass;
    // the hardware pass still reports the original 20-sprite line overflow.
    memset(&vdp,0,sizeof vdp);vdp.reg[1]=0x40;vdp.reg[12]=1;
    vdp.reg[2]=0x30;vdp.reg[4]=7;vdp.reg[5]=0x60;
    memset(vdp.vram+32,0x11,32);
    for(unsigned i=0;i<21;++i){
        word(0xc000+i*8,168);word(0xc002+i*8,i==20?0:i+1);
        word(0xc004+i*8,0x8001);word(0xc006+i*8,128+i*8);
    }
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);
    CHECK(row[160]==0&&vdp.sprite_overflow);
    gvdp_set_visual_sprites(identity,NULL);
    CHECK(gvdp_render_scanline(&vdp,40,row)==320);
    CHECK(row[160]==1&&vdp.sprite_overflow);
    gvdp_set_visual_sprites(NULL,NULL);
    // A finite road bitmap must not wrap its asphalt into the other margin
    // when the camera is beyond the left road edge (RR2/3 curve regression).
    memset(&vdp,0,sizeof vdp);vdp.reg[1]=0x40;vdp.reg[12]=1;
    vdp.reg[2]=0x10;vdp.reg[4]=3;vdp.reg[5]=0x70;vdp.reg[16]=3;
    vdp.reg[11]=3;vdp.reg[13]=0x30;
    memset(vdp.vram+32,0x11,32);memset(vdp.vram+64,0x22,32);
    for(int y=0;y<32;y++)for(int x=44;x<84;x++){
        word(0x4000+(y*128+x)*2,1);word(0x6000+(y*128+x)*2,2);
    }
    word(0xc000+140*4,0xfc00);word(0xc002+140*4,0x2a0);
    extern void gvdp_set_road_profile(int);
    gvdp_set_road_profile(2);gvdp_set_ws_extra(54);
    CHECK(gvdp_render_scanline(&vdp,140,row)==428);
    for(int x=374;x<428;x++)CHECK(row[x]==2);
    gvdp_set_ws_extra(0);gvdp_set_road_profile(0);
    // RR1 has opposite finite halves. The far edge of its grass mask must
    // continue as terrain instead of creating a second asphalt wedge. Apply
    // the correction only on an actual terrain row, preserving all sky tiles.
    memset(&vdp,0,sizeof vdp);vdp.reg[1]=0x40;vdp.reg[12]=1;
    vdp.reg[2]=0x10;vdp.reg[4]=3;vdp.reg[5]=0x70;vdp.reg[16]=3;
    vdp.reg[11]=3;vdp.reg[13]=0x30;
    memset(vdp.vram+32,0xaa,32);memset(vdp.vram+64,0x11,32);
    for(int x=0;x<128;++x){word(0x4000+(5*128+x)*2,0x2002);word(0x6000+(5*128+x)*2,0x2002);}
    for(int x=44;x<84;++x){word(0x4000+(18*128+x)*2,0x6001);word(0x6000+(18*128+x)*2,0x6001);}
    word(0xc000+144*4,684);word(0xc002+144*4,20);
    gvdp_set_ws_extra(54);gvdp_set_visual_surface(passthrough,NULL);
    uint8_t sky[GVDP_MAX_WIDTH];CHECK(gvdp_render_scanline(&vdp,40,sky)==428);
    gvdp_set_road_profile(1);CHECK(gvdp_render_scanline(&vdp,40,row)==428);
    CHECK(!memcmp(row,sky,428));
    CHECK(gvdp_render_scanline(&vdp,144,row)==428);CHECK(row[0]==58&&row[54]==58);
    // Pausing disables organic callbacks but keeps the wide canvas. Its outer
    // ground must still continue the finite mask, with a byte-exact native center.
    gvdp_set_visual_surface(NULL,NULL);gvdp_set_ws_extra(0);
    uint8_t paused_native[GVDP_MAX_WIDTH];
    CHECK(gvdp_render_scanline(&vdp,144,paused_native)==320);
    gvdp_set_ws_extra(54);CHECK(gvdp_render_scanline(&vdp,144,row)==428);
    CHECK(!memcmp(row+54,paused_native,320));
    for(int x=0;x<54;++x)CHECK(row[x]==58);
    gvdp_set_visual_surface(passthrough,NULL);
    // RR2's repeated B border used to leak into the center/native columns:
    // the old finite guard started only at the expanded right margin.
    word(0xc000+144*4,(unsigned)(uint16_t)-800);word(0xc002+144*4,672);
    vdp.reg[7]=57;gvdp_set_road_profile(2);gvdp_set_visual_scroll(road_shift,NULL);
    CHECK(gvdp_render_scanline(&vdp,144,row)==428);
    for(int x=54;x<428;++x)CHECK(row[x]==58); // native view + right margin are ground
    // Both source coordinates must use the visual translation at the join.
    // A sky row must still be byte-identical to the unmodified plane draw.
    CHECK(gvdp_render_scanline(&vdp,40,row)==428);CHECK(!memcmp(row,sky,428));
    gvdp_set_visual_scroll(NULL,NULL);
    gvdp_set_visual_surface(NULL,NULL);gvdp_set_ws_extra(0);gvdp_set_road_profile(0);
    // Sprite collisions confined to the expanded margin are not hardware
    // collisions. Wider rendering must preserve central pixels and flags.
    memset(&vdp,0,sizeof vdp);vdp.reg[1]=0x40;vdp.reg[12]=1;
    vdp.reg[2]=0x30;vdp.reg[4]=7;vdp.reg[5]=0x61;
    memset(vdp.vram+32,0x11,32);
    word(0xc000,128+40);word(0xc002,1);word(0xc004,0x8001);word(0xc006,128-40);
    word(0xc008,128+40);word(0xc00a,0);word(0xc00c,0x8001);word(0xc00e,128-40);
    uint8_t native[GVDP_MAX_WIDTH];
    vdp.sprite_collision=vdp.sprite_overflow=0;
    CHECK(gvdp_render_scanline(&vdp,40,native)==320);CHECK(!vdp.sprite_collision);
    gvdp_set_ws_extra(54);
    CHECK(gvdp_render_scanline(&vdp,40,row)==428);
    CHECK(!vdp.sprite_collision&&!vdp.sprite_overflow);
    CHECK(!memcmp(row+54,native,320));CHECK(row[14]==1);
    gvdp_evaluate_scanline(&vdp,40);CHECK(!vdp.sprite_collision);
    gvdp_set_ws_extra(0);
    vdp.scanline=0xea;phase=0;CHECK((gvdp_read_hv_counter(&vdp)>>8)==0xea);
    vdp.scanline=0xeb;CHECK((gvdp_read_hv_counter(&vdp)>>8)==0xe5);
    vdp.scanline=0;phase=1500;unsigned h1=gvdp_read_hv_counter(&vdp)&255;
    phase=2500;unsigned h2=gvdp_read_hv_counter(&vdp)&255;CHECK(h2!=h1);
    vdp.in_hblank=1;CHECK((gvdp_read_control(&vdp)&4)!=0);CHECK(vdp.in_hblank==1);
    puts("VDP: H40 SAT alignment, ignored sprite bits, expanded scale, HV progression, read-only HBlank passed");return 0;
}
