/* Per-ROM adapter. Enhanced renderer and new-player rules are separate work. */
#include "game_spec.h"
#include "genesis_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include "genesis_machine.h"
#include "ra_service.h"
#include "md_memory.h"
#include "input_script.h"
#include "overlay_draw.h"
#include "remix_player.h"
#include "audio.h"
#include "local_achievements.h"
#include "motion_bridge.h"
#include "geometry.h"
#include "scene_capture.h"
#include "sprite_geometry.h"
#include <SDL2/SDL_keycode.h>
#include <string.h>

#if RR_GAME_rr1
#define RR_NAME "Road Rash"
#define RR_SHORT "rr1"
#define RR_CRC 0xDEA53D19u
#define RR_SIZE 0xC0000u
#define RR_VINT 0x1164u
#define RR_HINT 0x145Au
#elif RR_GAME_rr2
#define RR_NAME "Road Rash II"
#define RR_SHORT "rr2"
#define RR_CRC 0x0876E992u
#define RR_SIZE 0x100000u
#define RR_VINT 0x11B8u
#define RR_HINT 0x14BEu
#else
#define RR_NAME "Road Rash 3"
#define RR_SHORT "rr3"
#define RR_CRC 0x15785956u
#define RR_SIZE 0x200000u
#define RR_VINT 0x21B6u
#define RR_HINT 0x24FAu
#endif

static uint64_t rr_irq_epoch;
static RRRa *rr_achievements;
static int rr_ra_initialized;
static uint32_t rr_toast[320 * 148];
static int rr_toast_visible;
static int rr_panel_visible;
static unsigned rr_picture;
static unsigned rr_saved_music_choice;
static int rr_native_effects_override;
static uint64_t rr_tick;
static unsigned rr_read16(unsigned a) { a&=65535;return ((unsigned)g_ram[a]<<8)|g_ram[(a+1)&65535]; }
static unsigned rr_read32(unsigned a) { return (rr_read16(a)<<16)|rr_read16(a+2); }
static unsigned rr_music_choice_address(void) {
#if RR_GAME_rr1
    return 0x05d6; /* TST.W at $1524; nonzero skips motor updates. */
#elif RR_GAME_rr2
    return 0xf594; /* TST.W at $15E6; nonzero skips motor updates. */
#else
    return 0x752c; /* TST.W at $21F8; zero skips the two motor updates. */
#endif
}
static unsigned rr_pause_address(void) {
#if RR_GAME_rr1
    return 0x3696; /* XOR #1 at $BF7E; $368E is the START debounce latch. */
#elif RR_GAME_rr2
    return 0x254e;
#else
    return 0x2548;
#endif
}
static int rr_paused(void){return rr_read16(rr_pause_address())!=0;}
/* Preserve the host cursor for native STOP commands while the game's actual
 * pause toggle is set; the original sound driver still receives its command. */
static int rr_stop_for_pause(void) {return rr_paused();}
static unsigned rr_player(void) {
#if RR_GAME_rr1
    return 0x1eaa;
#elif RR_GAME_rr2
    return 0x0a62;
#else
    return 0x09b2;
#endif
}
static int rr_racing(void) {
#if RR_GAME_rr1
    unsigned pointer=rr_read32(0x389c);
#elif RR_GAME_rr2
    unsigned pointer=rr_read32(0x2796);
#else
    unsigned pointer=rr_read32(0x276e);
#endif
    return pointer>=0xff0000&&pointer<0xffff00&&rr_read16(pointer+12)==0x8210&&g_machine.vdp.reg[2]==0x10;
}
static int rr_split_screen(void) {
#if RR_GAME_rr2
    return rr_read16(0x0304)!=0;
#elif RR_GAME_rr3
    return rr_read16(0x022c)==1;
#else
    return 0;
#endif
}
int rr_game_key(int key) {
    if(key==SDLK_F6) {rr_local_toggle();return 1;}
    return 0;
}
static void rr_frame_pre(uint64_t frame) {
    rr_tick=frame;
#if RR_GAME_rr1
    rr_geometry_source(0,g_rom,RR_SIZE,g_ram);
#elif RR_GAME_rr2
    rr_geometry_source(1,g_rom,RR_SIZE,g_ram);
#else
    rr_geometry_source(2,g_rom,RR_SIZE,g_ram);
#endif
    int road_scene=rr_racing()&&!rr_split_screen()&&!rr_local_panel();
    rr_geometry_frame(frame,road_scene?(rr_paused()?2:1):0);
    /* Use guest effects mode while the host supplies the remake, restoring
     * the user's option at the end of the simulated frame. */
    rr_native_effects_override=rr_racing()&&rr_music_replacing();
    if(rr_native_effects_override) {
        unsigned address=rr_music_choice_address();
        rr_saved_music_choice=rr_read16(address);
        g_ram[address]=0;
#if RR_GAME_rr3
        g_ram[address+1]=1;
#else
        g_ram[address+1]=0;
#endif
    }
    const char *wide=getenv("RR_WIDE");
    int on=wide&&*wide=='1';
    extern void gvdp_set_road_profile(int);
#if RR_GAME_rr1
    gvdp_set_road_profile(1);
#elif RR_GAME_rr2
    gvdp_set_road_profile(2);
#else
    gvdp_set_road_profile(3);
#endif
#if RR_GAME_rr1
    rr_picture=rr_read32(0x389c);
#elif RR_GAME_rr2
    rr_picture=rr_read32(0x2796);
    if(rr_read16(0x0304))on=0;
#else
    rr_picture=rr_read32(0x276e);
    if(rr_read16(0x022c)==1)on=0;
#endif
    int native_width=gvdp_screen_width(&g_machine.vdp);
    int margin=rr_wide_margin(native_width);
    gvdp_set_ws_canvas(on?native_width+2*margin:0);
    gvdp_set_ws_extra(on&&rr_racing()?margin:0);
    gvdp_set_ws_bar_black(1);
    if(frame==0)fprintf(stderr,"[RR CONFIG] game=%s version=%s requested_wide=%d music_volume=%s effects_volume=%s preset=%s\n",
        RR_SHORT,GENESIS_GAME_VERSION,on,getenv("RR_MUSIC_VOLUME")?getenv("RR_MUSIC_VOLUME"):"20",getenv("RR_EFFECTS_VOLUME")?getenv("RR_EFFECTS_VOLUME"):"100",getenv("RR_SHADER_PATH")?getenv("RR_SHADER_PATH"):"none");
}
static void rr_shutdown(void) { rr_ra_destroy(rr_achievements); rr_achievements = NULL; rr_music_shutdown(); }
static const char *rr_no_states(void) { return "Road Rash snapshots are not qualified yet"; }
static void rr_entry(void) { recomp_call_addr(0x200u); }
static void rr_vblank(void) { recomp_call_addr(RR_VINT); ++rr_irq_epoch; }
static void rr_hblank(void) { recomp_call_addr(RR_HINT); ++rr_irq_epoch; }

/* v0.1.2 declares/emits this ABI but has no definition. In these cartridge
   revisions STOP appears in exception handlers with SR=$2700 (all maskable
   interrupts disabled). Keep that halt visible and let the host stay alive.
   For an interruptible STOP the accepted per-game IRQ handlers wake the wait. */
void genesis_stop_until_interrupt(uint16_t sr_imm)
{
    uint64_t epoch = rr_irq_epoch;
    g_cpu.SR = sr_imm;
    fprintf(stderr, "[RR STOP] SR=%04X PC=%06X; CPU waiting for accepted IRQ\n", sr_imm, g_cpu.PC);
    do { glue_yield_for_interrupt_poll(); } while (rr_irq_epoch == epoch);
}

static void rr_frame_post(uint64_t frame)
{
    if(rr_native_effects_override) {
        unsigned address=rr_music_choice_address();
        g_ram[address]=(uint8_t)(rr_saved_music_choice>>8);
        g_ram[address+1]=(uint8_t)rr_saved_music_choice;
        rr_native_effects_override=0;
    }
    if (!rr_ra_initialized) {
        rr_ra_initialized = 1;
        atexit(rr_shutdown);
        rr_music_init(RR_SHORT); audio_set_extra_source(rr_music_mix);
        rr_local_init(RR_SHORT,!input_script_active());
        if (!input_script_active()) {
            extern uint32_t rr_ra_read(uint32_t, uint8_t *, uint32_t, void *);
            rr_achievements = rr_ra_game_create(g_rom, RR_SIZE, rr_ra_read, NULL);
        }
    }
    rr_ra_update(rr_achievements, 1);
    int paused=rr_paused();
    rr_music_pause(paused);
    rr_motion_scene(rr_racing()&&!paused&&!rr_local_panel(),rr_picture);
    rr_local_frame(rr_racing(),rr_read16(rr_player()+18),0,paused);
    const char *notification = rr_ra_notification(rr_achievements);
    if(!notification||!*notification)notification=rr_local_toast();
    rr_toast_visible = notification && *notification;
    if (rr_toast_visible) {
        gen_ovl_fill_rect(rr_toast, 320, 30, 0, 0, 320, 30, 0xFF171B1Eu);
        gen_ovl_draw_text(rr_toast, 320, 30, 7, 4, "CONQUISTA DESBLOQUEADA", 0xFFFFCD35u, 1);
        gen_ovl_draw_text(rr_toast, 320, 30, 7, 16, notification, 0xFFF5F1E5u, 1);
    }
    rr_panel_visible=rr_local_panel();
    if(rr_panel_visible) {
        gen_ovl_fill_rect(rr_toast,320,148,0,0,320,148,0xFF171B1E);
        gen_ovl_draw_text(rr_toast,320,148,8,8,"CONQUISTAS LOCAIS",0xFFFFCD35,1);
        gen_ovl_draw_text(rr_toast,320,148,8,22,"Sem conta - progresso neste computador",0xFFF4F0E2,1);
        for(unsigned i=0;i<rr_local_count();++i){
            gen_ovl_draw_text(rr_toast,320,148,8,40+i*14,rr_local_unlocked(i)?"[X]":"[ ]",0xFFFFCD35,1);
            gen_ovl_draw_text(rr_toast,320,148,32,40+i*14,rr_local_title(i),0xFFF4F0E2,1);
        }
        gen_ovl_draw_text(rr_toast,320,148,8,132,"F6: fechar",0xFFADB2AB,1);
    }
    if (frame % 300u == 0 && getenv("RR_TRACE")) {
        fprintf(stderr, "[RR TRACE] frame=%llu PC=%06X SR=%04X A7=%08X VDP1=%02X Z80pc=%04X request=%d flag0B=%02X native=%llu\n",
                (unsigned long long)frame, g_cpu.PC, g_cpu.SR, g_cpu.A[7],
                g_machine.vdp.reg[1], g_machine.z80.pc, g_machine.bus.z80_busreq,
                g_machine.bus.z80_ram[0xB], (unsigned long long)g_native_insn_count);
        fprintf(stderr,"[RR VDP] regs=");
        for(int i=0;i<24;++i)fprintf(stderr,"%02X",g_machine.vdp.reg[i]);
        fprintf(stderr," vs=%04X,%04X scan=%u\n",g_machine.vdp.vsram[0],g_machine.vdp.vsram[1],g_machine.vdp.scanline);
    }
}

static int rr_hook(uint32_t pc) {
    static unsigned music_cue;
    static int stopping_native;
#if RR_GAME_rr1
    if(pc==0xf44c){rr_geometry_actor_end(0,g_cpu.A[0],g_ram);return 0;}
    if(pc==0xf4ca){rr_geometry_actor_begin(0,g_cpu.A[0],g_cpu.A[1]);return 0;}
    if(pc==0xfe3a){rr_geometry_project(0,g_cpu.A[0],g_cpu.A[1],g_cpu.D[0],(int16_t)g_cpu.D[1],(int16_t)g_cpu.D[2]);return 0;}
    if(pc==0x10c7e){rr_geometry_scenery(0,g_cpu.A[0],g_cpu.A[1],g_cpu.D[6],g_ram);return 0;}
    if(pc==0x9484){rr_geometry_job_begin(0,g_cpu.A[0],g_ram);rr_sprite_job_begin(RR_SHORT,rr_tick,g_cpu.A[0],g_ram,0xde64,0x38ac);return 0;}
    if(pc==0x9476){rr_geometry_job_end(g_ram);rr_sprite_job_end(g_ram,0xde64);return 0;}
    if(pc==0xdc58){rr_geometry_actor(0,g_cpu.A[0],g_cpu.A[1],g_ram);rr_scene_capture(RR_SHORT,rr_tick,g_cpu.A[1],g_cpu.A[0],0xff1e68,g_ram,20);return 0;}
#elif RR_GAME_rr2
    if(pc==0x156b6){rr_geometry_actor_end(1,g_cpu.A[0],g_ram);return 0;}
    if(pc==0x15734){rr_geometry_actor_begin(1,g_cpu.A[0],g_cpu.A[1]);return 0;}
    if(pc==0x1612a){rr_geometry_project(1,g_cpu.A[0],g_cpu.A[1],g_cpu.D[0],(int16_t)g_cpu.D[1],(int16_t)g_cpu.D[2]);return 0;}
    if(pc==0x171c8){rr_geometry_scenery(1,g_cpu.A[0],g_cpu.A[1],g_cpu.D[6],g_ram);return 0;}
    if(pc==0xa5c8){rr_geometry_job_begin(1,g_cpu.A[0],g_ram);rr_sprite_job_begin(RR_SHORT,rr_tick,g_cpu.A[0],g_ram,0xdc34,0x27a6);return 0;}
    if(pc==0xa5b8||pc==0x26eda){rr_geometry_job_end(g_ram);rr_sprite_job_end(g_ram,0xdc34);return 0;}
    if(pc==0x12642){rr_geometry_actor(1,g_cpu.A[0],g_cpu.A[1],g_ram);rr_scene_capture(RR_SHORT,rr_tick,g_cpu.A[1],g_cpu.A[0],0xff0a0a,g_ram,22);return 0;}
#else
    if(pc==0x1afac){rr_geometry_actor_end(2,g_cpu.A[0],g_ram);return 0;}
    if(pc==0x1b032){rr_geometry_actor_begin(2,g_cpu.A[0],g_cpu.A[1]);return 0;}
    if(pc==0x1bdb8){rr_geometry_project(2,g_cpu.A[0],g_cpu.A[1],g_cpu.D[0],(int16_t)g_cpu.D[1],(int16_t)g_cpu.D[2]);return 0;}
    if(pc==0x1cb96){rr_geometry_scenery(2,g_cpu.A[0],g_cpu.A[1],g_cpu.D[6],g_ram);return 0;}
    if(pc==0x1c24){rr_geometry_job_begin(2,g_cpu.A[0],g_ram);rr_sprite_job_begin(RR_SHORT,rr_tick,g_cpu.A[0],g_ram,0x4fc2,0x277e);return 0;}
    if(pc==0x29160||pc==0x29b16){rr_geometry_job_end(g_ram);rr_sprite_job_end(g_ram,0x4fc2);return 0;}
    if(pc==0x17df6){rr_geometry_actor(2,g_cpu.A[0],g_cpu.A[1],g_ram);rr_scene_capture(RR_SHORT,rr_tick,g_cpu.A[1],g_cpu.A[0],0xff0956,g_ram,32);return 0;}
    if(pc==0x10b14&&rr_music_replacing()) {
        /* Motor voices must be allocated by the race loader, before the
         * first racing frame. Later per-frame frequency updates cannot
         * create voices that were skipped here. */
        if(!rr_native_effects_override)rr_saved_music_choice=rr_read16(0x752c);
        rr_native_effects_override=1;
        g_ram[0x752c]=0;g_ram[0x752d]=1;
        return 0;
    }
#endif
    if(getenv("RR_TRACE"))fprintf(stderr,"[RR HOOK] pc=%06X D0=%08X D1=%08X parent=%06X\n",pc,g_cpu.D[0],g_cpu.D[1],rr_read32(g_cpu.A[7]+8));
#if RR_GAME_rr1 || RR_GAME_rr2
    if(pc==0x1a122||pc==0x2be64) {
        if(rr_music_start(g_cpu.D[0]&0xffff)) {
            g_ram[0x1fd]=1; /* original driver's BGM stop request; SFX stay live */
            return 1;
        }
    }else if(pc==0x1a1fa||pc==0x2bf3c) {
        if(rr_stop_for_pause())rr_music_pause(1);else rr_music_stop();
    }
#else
    unsigned cue=g_cpu.D[1]&0xffff;
    if(pc==0x3a9de&&cue>=1&&cue<=11) {
        if(rr_music_start(cue)) {
            M68KState saved=g_cpu;
            if(music_cue){stopping_native=1;g_cpu.D[1]=music_cue;recomp_call_addr(0x3aade);stopping_native=0;}
            g_cpu=saved;music_cue=cue;g_cpu.D[0]=0;
            return 1;
        }
        music_cue=cue;
    }else if(pc==0x3aade&&cue==music_cue&&!stopping_native) {
        if(rr_stop_for_pause())rr_music_pause(1);else rr_music_stop();
    }
#endif
    return 0;
}

/* The published MD achievement sets use the libretro word-swizzled view:
   logical RA 0..FFFF maps to 68000 FF0000..FFFFFF, with byte lane XOR 1.
   Never use m68k_read* here: evaluation must not consume bus cycles. */
uint32_t rr_ra_read(uint32_t address, uint8_t *out, uint32_t count, void *user)
{
    (void)user;
    return rr_md_ra_read(g_ram, address, out, count);
}
static int rr_video_configure(const char *mode) { return !strcmp(mode, "off"); }
static int rr_video_enabled(void) { return 0; }
static int rr_video_width(int dw, int dh, int nw, int nh) { (void)dw; (void)dh; (void)nw; (void)nh; return 0; }
static void rr_overlay(const struct GVDP *vdp, int line, uint32_t *out, int width)
{
    (void)vdp;
    if(width>320&&line>=152){
        int margin=(width-320)/2;
        for(int x=0;x<margin;++x)out[x]=out[width-1-x]=0xff101315;
    }
    if (rr_panel_visible && line>=38&&line<186) {
        int start=(width-320)/2;
        for(int x=0;x<320;++x)if(start+x>=0&&start+x<width)out[start+x]=rr_toast[(line-38)*320+x];
    }else if (rr_toast_visible && line >= 6 && line < 36) {
        int start = (width - 320) / 2;
        for (int x = 0; x < 320; ++x) if (start + x >= 0 && start + x < width)
            out[start + x] = rr_toast[(line - 6) * 320 + x];
    }
}
static const GameVideo rr_video = {
    .configure = rr_video_configure, .enabled = rr_video_enabled,
    .width = rr_video_width, .overlay_scanline = rr_overlay,
};

const GameSpec g_game_spec = {
    .video = &rr_video,
    .display_name = RR_NAME,
    .short_name = RR_SHORT,
    .expected_rom_crc32 = RR_CRC,
    .expected_rom_size = RR_SIZE,
    /* Initial integrations retain measured fallback coverage. The launcher
       must not describe this as a fully static, coverage-complete release. */
    .tier3_floor_default = 1,
    .state_unavailable_reason = rr_no_states,
    .call_entry_point = rr_entry,
    .call_vblank = rr_vblank,
    .call_hblank = rr_hblank,
    .on_frame_post = rr_frame_post,
    .on_frame_pre = rr_frame_pre,
    .instruction_hook = rr_hook,
};
