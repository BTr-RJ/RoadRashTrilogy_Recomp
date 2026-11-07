"""Add presentation-only affine sprite sampling after authentic VDP evaluation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
directory = ROOT / '.tools/engine/runner/video'
header = directory / 'genesis_vdp.h'
text = header.read_text(encoding='utf-8')
if 'GVDPVisualSprite' not in text:
    anchor = '/* ---- 68K port interface'
    at = text.index(anchor)
    text = text[:at] + '''/* RR_VISUAL_SPRITES: continuous object rectangles in native screen units.
 * The callback runs only in the presentation pass. Original SAT evaluation
 * still supplies every guest collision/overflow flag. No hardware memory is
 * changed, including X/Y, size, tiles, palette and links. */
typedef int (*GVDPVisualSprite)(void *user,const struct GVDP *v,
                              unsigned sat_index,double bounds[4]);
void gvdp_set_visual_sprites(GVDPVisualSprite transform,void *user);

''' + text[at:]
    header.write_text(text, encoding='utf-8')
text = header.read_text(encoding='utf-8')
if 'GVDPVisualScroll' not in text:
    text = text.replace('/* ---- 68K port interface', '''/* Presentation-only tile-plane coordinates: hscroll A/B, vscroll A/B.
 * Only the presentation pass consumes these values; register/RAM reads and
 * authentic raster evaluation retain the guest values. */
typedef int (*GVDPVisualScroll)(void *user,const struct GVDP *v,int line,int values[4]);
void gvdp_set_visual_scroll(GVDPVisualScroll transform,void *user);

/* ---- 68K port interface''')
    header.write_text(text, encoding='utf-8')

path = directory / 'genesis_vdp.c'
text = path.read_text(encoding='utf-8')
if 'RR_VISUAL_SPRITES' not in text:
    text = text.replace('#include "genesis_vdp.h"', '#include "genesis_vdp.h"\n#include <math.h>')
    anchor = 'static GVDPHostSprites s_host_sprites;'
    assert text.count(anchor) == 1
    text = text.replace(anchor, '''/* RR_VISUAL_SPRITES */
static GVDPVisualSprite s_visual_sprites;
static void *s_visual_user;
static int s_visual_pass;
void gvdp_set_visual_sprites(GVDPVisualSprite transform,void *user)
{ s_visual_sprites=transform;s_visual_user=user; }
''' + anchor)
    anchor = '        if (line >= y && line < y + height) {'
    assert text.count(anchor) == 1
    text = text.replace(anchor, '''        double rectangle[4];
        if(s_visual_pass && s_visual_sprites && !im2 &&
           s_visual_sprites(s_visual_user,v,(unsigned)link,rectangle)) {
            double left=rectangle[0]+offset,top=rectangle[1];
            double right=rectangle[2]+offset,bottom=rectangle[3];
            int source_width=hsz*8,source_height=vsz*8;
            if(isfinite(left)&&isfinite(top)&&isfinite(right)&&isfinite(bottom)&&
               right>left&&bottom>top&&right-left<2048&&bottom-top<2048&&
               line+.5>=top&&line+.5<bottom) {
                uint16_t attr=vram_read_word(v,(uint16_t)(e+4));
                int tile=attr&2047,pal=(attr>>13)&3,pri=(attr>>15)&1;
                int hf=(attr&0x0800)!=0,vf=(attr&0x1000)!=0;
                int source_y=(int)floor((line+.5-top)*source_height/(bottom-top));
                if(vf)source_y=source_height-1-source_y;
                int begin=(int)fmax(0,ceil(left-.5)),end=(int)fmin(total,ceil(right-.5));
                for(int sx=begin;sx<end;++sx) {
                    if(drawn[sx])continue;
                    int source_x=(int)floor((sx+.5-left)*source_width/(right-left));
                    if(hf)source_x=source_width-1-source_x;
                    if(source_x<0||source_x>=source_width||source_y<0||source_y>=source_height)continue;
                    unsigned tileno=(tile+(source_x>>3)*vsz+(source_y>>3))&2047;
                    unsigned address=tileno*32+(source_y&7)*4+((source_x&7)>>1);
                    unsigned byte=patterns?(address<pattern_bytes?patterns[address]:0):v->vram[(uint16_t)address];
                    unsigned nib=(byte>>((source_x&1)?0:4))&15;
                    if(!nib)continue;
                    if(sh_mode&&pal==3&&nib==14)s_spr_hilite_op[sx]=1;
                    else if(sh_mode&&pal==3&&nib==15)s_spr_shadow_op[sx]=1;
                    else {
                        int ext=pal==0?gvdp_sprite_pattern_line(sz,tileno):0;
                        s_spr_idx[sx]=ext?(uint8_t)(GVDP_HOST_PALETTE_BASE+(ext-1)*16+nib):(uint8_t)(pal*16+nib);
                        s_spr_op[sx]=1;s_spr_hi[sx]=(uint8_t)pri;drawn[sx]=1;
                    }
                }
            }
            if(!next)break;
            link=next;continue;
        }

''' + anchor)
    begin = text.index('/* RR_WIDE_CANON:')
    text = text[:begin] + '''/* RR_WIDE_CANON: guest status always comes from native SAT geometry.
 * RR_VISUAL_SPRITES applies object rectangles only in the second pass. */
int gvdp_render_scanline(GVDP *v,int line,uint8_t *out) {
    int extra=s_ws_extra,canvas=s_ws_canvas_w;
    if(extra<=0&&canvas<=0&&!s_visual_sprites&&!s_visual_scroll)return rr_render_scanline(v,line,out);
    uint8_t native[GVDP_MAX_WIDTH];
    s_visual_pass=0;s_ws_extra=0;s_ws_canvas_w=0;
    int width=rr_render_scanline(v,line,native);
    uint8_t collision=v->sprite_collision,overflow=v->sprite_overflow;
    s_ws_extra=extra;s_ws_canvas_w=canvas;s_visual_pass=1;
    int total=rr_render_scanline(v,line,out);
    s_visual_pass=0;
    if(!s_visual_sprites&&!s_visual_scroll)memcpy(out+(total-width)/2,native,(size_t)width);
    v->sprite_collision=collision;v->sprite_overflow=overflow;
    return total;
}
'''
    path.write_text(text, encoding='utf-8')
text = path.read_text(encoding='utf-8')
if 'RR_VISUAL_SCROLL' not in text:
    text = text.replace('static int s_visual_pass;', '''static int s_visual_pass;
/* RR_VISUAL_SCROLL */
static GVDPVisualScroll s_visual_scroll;
static void *s_visual_scroll_user;
void gvdp_set_visual_scroll(GVDPVisualScroll transform,void *user)
{s_visual_scroll=transform;s_visual_scroll_user=user;}''')
    anchor = '    int full_vs_b = v->vsram[1] & vs_mask;'
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '''
    if(s_visual_pass&&s_visual_scroll&&!im2&&!vmode_2cell){
        int values[4]={hs_a,hs_b,full_vs_a,full_vs_b};
        if(s_visual_scroll(s_visual_scroll_user,v,line,values)){
            hs_a=values[0];hs_b=values[1];full_vs_a=values[2];full_vs_b=values[3];
        }
    }''')
    path.write_text(text, encoding='utf-8')
print('Applied presentation-only continuous sprite geometry')

text=header.read_text(encoding='utf-8')
if 'gvdp_set_visual_objects' not in text:
    text=text.replace('void gvdp_set_visual_sprites(GVDPVisualSprite transform,void *user);',
        'void gvdp_set_visual_sprites(GVDPVisualSprite transform,void *user);\n'
        '/* Full source-art billboards, only in the visual pass. */\n'
        'void gvdp_set_visual_objects(GVDPHostSprites draw,void *user);')
    header.write_text(text,encoding='utf-8')
text=path.read_text(encoding='utf-8')
if 'RR_SOURCE_OBJECTS' not in text:
    text=text.replace('static int s_visual_pass;', '''static int s_visual_pass;
/* RR_SOURCE_OBJECTS */
static GVDPHostSprites s_visual_objects;
static void *s_visual_objects_user;
void gvdp_set_visual_objects(GVDPHostSprites draw,void *user)
{s_visual_objects=draw;s_visual_objects_user=user;}''')
    anchor='    sprite_render_line(v, line, total, offset);'
    assert text.count(anchor)==1
    text=text.replace(anchor,anchor+'''
    if(s_visual_pass&&s_visual_objects){
        GVDPSpriteLayer layer={line,total,offset,s_spr_idx,s_spr_op,s_spr_hi};
        s_visual_objects(s_visual_objects_user,v,&layer);
    }''')
    path.write_text(text,encoding='utf-8')

text=header.read_text(encoding='utf-8')
if 'GVDPVisualSurface' not in text:
    text=text.replace('/* ---- 68K port interface', '''/* Material sampling before composition; never invoked on a visible sprite. */
typedef int (*GVDPVisualSurface)(void *user,const struct GVDP *v,int line,int x,int index);
void gvdp_set_visual_surface(GVDPVisualSurface draw,void *user);

/* ---- 68K port interface''')
    header.write_text(text,encoding='utf-8')
text=path.read_text(encoding='utf-8')
if 'RR_WORLD_SURFACE' not in text:
    text=text.replace('static int s_visual_pass;', '''static int s_visual_pass;
/* RR_WORLD_SURFACE */
static GVDPVisualSurface s_visual_surface;
static void *s_visual_surface_user;
void gvdp_set_visual_surface(GVDPVisualSurface draw,void *user)
{s_visual_surface=draw;s_visual_surface_user=user;}''')
    anchor='        /* Shadow/highlight: when enabled, a pixel with no high-priority layer'
    assert text.count(anchor)==1
    text=text.replace(anchor,'''        if(s_visual_pass&&s_visual_surface&&
           !(s_op&&(s_hi||!((a_op&&a_hi)||(b_op&&b_hi)))))
            px=(uint8_t)s_visual_surface(s_visual_surface_user,v,line,x,px);

'''+anchor)
    path.write_text(text,encoding='utf-8')

# A visual-only object or material callback must also request the second pass.
text=path.read_text(encoding='utf-8')
text=text.replace('!s_visual_sprites&&!s_visual_scroll)',
    '!s_visual_sprites&&!s_visual_scroll&&!s_visual_objects&&!s_visual_surface)')
path.write_text(text,encoding='utf-8')

# Optional scanline audit uses pixels/owners at raster time. End-of-frame VRAM
# dumps can contain a later animation upload and cannot establish pixel masks.
text=header.read_text(encoding='utf-8')
if 'GVDPVisualAudit' not in text:
    text=text.replace('/* ---- 68K port interface', '''typedef struct GVDPVisualAuditRow {
    int line,native_width,total,offset;
    const uint8_t *native_index,*native_owner,*index,*owner;
} GVDPVisualAuditRow;
typedef void (*GVDPVisualAudit)(void *user,const struct GVDP *v,const GVDPVisualAuditRow *row);
void gvdp_set_visual_audit(GVDPVisualAudit audit,void *user);

/* ---- 68K port interface''')
    header.write_text(text,encoding='utf-8')
text=path.read_text(encoding='utf-8')
if 'RR_VISUAL_AUDIT' not in text:
    import re
    text=text.replace('static int s_visual_pass;', '''static int s_visual_pass;
/* RR_VISUAL_AUDIT */
static GVDPVisualAudit s_visual_audit;
static void *s_visual_audit_user;
static uint8_t s_spr_owner[GVDP_MAX_WIDTH],s_visible_owner[GVDP_MAX_WIDTH];
void gvdp_set_visual_audit(GVDPVisualAudit audit,void *user)
{s_visual_audit=audit;s_visual_audit_user=user;}''')
    text=text.replace('    memset(s_spr_op, 0, (size_t)total);',
        '    memset(s_spr_op, 0, (size_t)total);\n    memset(s_spr_owner,0,(size_t)total);')
    text=re.sub(r'(s_spr_op\[sx\]\s*=\s*1;)',r'\1s_spr_owner[sx]=(uint8_t)(link+1);',text)
    anchor='        if(s_visual_pass&&s_visual_surface&&'
    text=text.replace(anchor,'''        if(s_visual_audit)s_visible_owner[xo]=
            s_op&&(s_hi||!((a_op&&a_hi)||(b_op&&b_hi)))?s_spr_owner[xo]:0;
'''+anchor)
    text=text.replace('    int w = gvdp_screen_width(v);\n    int content_extra',
        '    if(s_visual_audit)memset(s_visible_owner,0,sizeof s_visible_owner);\n'
        '    int w = gvdp_screen_width(v);\n    int content_extra')
    text=text.replace('    int width=rr_render_scanline(v,line,native);',
        '    int width=rr_render_scanline(v,line,native);\n'
        '    uint8_t owners[GVDP_MAX_WIDTH];\n'
        '    if(s_visual_audit)memcpy(owners,s_visible_owner,(size_t)width);')
    text=text.replace('    v->sprite_collision=collision;v->sprite_overflow=overflow;',
        '    v->sprite_collision=collision;v->sprite_overflow=overflow;\n'
        '    if(s_visual_audit){GVDPVisualAuditRow row={line,width,total,(total-width)/2,'
        'native,owners,out,s_visible_owner};s_visual_audit(s_visual_audit_user,v,&row);}')
    path.write_text(text,encoding='utf-8')

# Host assemblies keep the native SAT first-wins order, including overlaps
# with sprites that remain in the hardware layer.
text=header.read_text(encoding='utf-8')
if 'uint8_t *owner;' not in text:
    text=text.replace('    uint8_t *high;    /* sprite priority bit */',
        '    uint8_t *high;    /* sprite priority bit */\n'
        '    uint8_t *owner;   /* optional SAT order: 1..80; lower draws first */')
    header.write_text(text,encoding='utf-8')
text=path.read_text(encoding='utf-8')
text=text.replace('GVDPSpriteLayer layer={line,total,offset,s_spr_idx,s_spr_op,s_spr_hi};',
    'GVDPSpriteLayer layer={line,total,offset,s_spr_idx,s_spr_op,s_spr_hi,s_spr_owner};')
path.write_text(text,encoding='utf-8')

# RR1's two finite grass masks occupy source columns 320..703. Beyond that
# strip the plane wraps to unrelated data/transparent asphalt in wide views.
text=path.read_text(encoding='utf-8')
if 'RR1_FINITE_GROUND' not in text and 'RR_FINITE_PAIR' not in text:
    row_anchor='    for (int x = -content_extra, xo = content_start; xo < content_end; x++, xo++) {'
    assert text.count(row_anchor)==1
    text=text.replace(row_anchor,'''#if !GENESIS_BATCHED_PLANES
    int rr1_ground_a=0,rr1_ground_b=0;
    if(rr_road_profile==1&&s_visual_pass&&(content_extra||s_visual_surface)&&wt==128&&base_a==0x4000&&base_b==0x6000&&
       hmode==3&&!im2&&!vmode_2cell&&line<152){
        rr1_ground_a=rr1_ground_b=1;
        for(int n=0;n<8;++n){
            uint8_t idx;int op,hi;
            fetch_plane_pixel(v,&cache_a,base_a,wt,ht,480+n,line+full_vs_a,im2,&idx,&op,&hi);
            rr1_ground_a&=op&&(idx==58||idx==59);
            fetch_plane_pixel(v,&cache_b,base_b,wt,ht,480+n,line+full_vs_b,im2,&idx,&op,&hi);
            rr1_ground_b&=op&&(idx==58||idx==59);
        }
    }
#endif
'''+row_anchor)
    anchor='        /* Sprite layer (indexed in output-column space). */'
    assert text.count(anchor)==1
    text=text.replace(anchor,'''#if !GENESIS_BATCHED_PLANES
        /* RR1_FINITE_GROUND: extend the authored terrain at a finite edge. */
        if(rr_road_profile==1&&s_visual_pass&&wt==128&&base_a==0x4000&&base_b==0x6000&&
           hmode==3&&!im2&&(x<0||x>=w||s_visual_surface)&&line<152){
            int ax=x-(int16_t)hs_a+1024,bx=x-(int16_t)hs_b;
            /* Hscroll is a 10-bit fetch coordinate; retain the raw word's
             * representative when presentation added a small translation. */
            ax=x-raw_a+(((raw_a-hs_a+512)&1023)-512)+1024;
            bx=x-raw_b+(((raw_b-hs_b+512)&1023)-512);
            if(rr1_ground_a){
                if(ax<512)fetch_plane_pixel(v,&cache_a,base_a,wt,ht,
                    480+(ax&7),line+vs_a,im2,&a_idx,&a_op,&a_hi);
                else if(ax>=704){a_idx=0;a_op=0;a_hi=0;}
            }
            if(rr1_ground_b){
                if(bx>=512)fetch_plane_pixel(v,&cache_b,base_b,wt,ht,
                    480+(bx&7),line+vs_b,im2,&b_idx,&b_op,&b_hi);
                else if(bx<320){b_idx=0;b_op=0;b_hi=0;}
            }
        }
#endif
'''+anchor)
    path.write_text(text,encoding='utf-8')

# Migrate the one-sided 0.4.2 guard to a common finite pair in RR1/RR2. RR3
# keeps its qualified legacy expansion. In particular, use the transformed
# scroll in EVERY visual column, not raw scroll only after x reaches 320.
text=path.read_text(encoding='utf-8')
text=text.replace('if(rr_road_profile==2 && wt==128',
                  'if(rr_road_profile>=2 && wt==128')
if 'RR_FINITE_PAIR' not in text:
    begin=text.index('    int rr1_ground_a=0,rr1_ground_b=0;')
    end=text.index('#endif',begin)
    text=text[:begin]+'''    int rr_ground_a=0,rr_ground_b=0;
    if((rr_road_profile==1||rr_road_profile==2)&&s_visual_pass&&(s_visual_surface||content_extra)&&
       wt==128&&base_a==0x4000&&base_b==0x6000&&hmode==3&&!im2&&!vmode_2cell&&line<152){
        rr_ground_a=rr_ground_b=1;
        for(int n=0;n<8;++n){
            uint8_t idx;int op,hi;
            fetch_plane_pixel(v,&cache_a,base_a,wt,ht,480+n,line+full_vs_a,im2,&idx,&op,&hi);
            rr_ground_a&=op&&(idx==58||idx==59);
            fetch_plane_pixel(v,&cache_b,base_b,wt,ht,480+n,line+full_vs_b,im2,&idx,&op,&hi);
            rr_ground_b&=op&&(idx==58||idx==59);
        }
    }
'''+text[end:]
    begin=text.index('        /* RR1_FINITE_GROUND:')
    end=text.index('#endif',begin)
    text=text[:begin]+'''        /* RR_FINITE_PAIR: each bitmap contributes ONE outward half.
         * The opposite half must never wrap back into the road or margins. */
        if((rr_ground_a||rr_ground_b)&&s_visual_pass&&(s_visual_surface||x<0||x>=w)){
            int sa=raw_a+(((hs_a-raw_a+512)&1023)-512);
            int sb=raw_b+(((hs_b-raw_b+512)&1023)-512);
            int ax=x-sa+(rr_road_profile==1?1024:0);
            int bx=x-sb+(rr_road_profile==1?0:1024);
            if(rr_ground_a){
                if(ax<512)fetch_plane_pixel(v,&cache_a,base_a,wt,ht,
                    480+(ax&7),line+vs_a,im2,&a_idx,&a_op,&a_hi);
                else if(ax>=704){a_idx=0;a_op=0;a_hi=0;}
            }
            if(rr_ground_b){
                if(bx>=512)fetch_plane_pixel(v,&cache_b,base_b,wt,ht,
                    480+(bx&7),line+vs_b,im2,&b_idx,&b_op,&b_hi);
                else if(bx<320){b_idx=0;b_op=0;b_hi=0;}
            }
        }
'''+text[end:]
path.write_text(text,encoding='utf-8')

