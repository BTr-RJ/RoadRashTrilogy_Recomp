#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL.h>
#include "game/campaign_state.h"
#include "music/remix_player.h"
#include "presentation/continuous_value.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
extern "C" {
#include "input_map.h"
#include "gamepad.h"
static unsigned saves,loads;
static std::string last_path;
int runner_save_state_file(const char *path){++saves;last_path=path;return 1;}
int runner_load_state_file(const char *path){++loads;last_path=path;return 1;}
bool input_script_active(){return false;}
int glue_state_boundary_ready(){return 0;}
size_t glue_rb_sched_save(void *,size_t){return 16;}
int glue_rb_sched_load(const void *,size_t){return 1;}
size_t audio_event_state_size(){return 16;}
int audio_event_state_save(void *,size_t){return 1;}
int audio_event_state_load(const void *,size_t,int){return 1;}
size_t machine_rb_save(void *,size_t){return 16;}
int machine_rb_load(const void *,size_t){return 1;}
void rr_state_resume(){}
void rr_adapter_state(uint64_t *,unsigned *,int){}
const void *genesis_sim_input(){static char input[64];return input;}
void genesis_sim_input_restore(const void *){}
uint32_t genesis_sim_tick_count(){return 0;}
void genesis_sim_set_tick_count(uint32_t){}
void rr_music_capture(RRMusicState *state){*state={};}
int rr_music_restore(const RRMusicState *,int){return 1;}
}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"campaign line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void poll(){SDL_JoystickUpdate();SDL_Event ev;while(SDL_PollEvent(&ev))gamepad_handle_event(&ev);rr_campaign_input();}
int main(){
    auto root=std::filesystem::temp_directory_path()/(L"rr-campaign-"+std::to_wstring(GetCurrentProcessId()));
    _wputenv_s(L"RR_ROOT",root.c_str());rr_campaign_init("rr2");
    for(int i=0;i<500;++i)rr_campaign_observe(0,1,0);rr_campaign_tick();CHECK(saves==0);
    for(int i=0;i<179;++i)rr_campaign_observe(1,1,1);
    for(int i=0;i<100;++i)rr_campaign_observe(0,1,0);rr_campaign_tick();CHECK(saves==0);
    rr_campaign_init("rr2");
    for(int i=0;i<180;++i)rr_campaign_observe(1,1,1);
    for(int i=0;i<89;++i)rr_campaign_observe(0,1,0);rr_campaign_tick();CHECK(saves==0);
    rr_campaign_observe(0,1,0);rr_campaign_tick();CHECK(saves==1);
    for(int i=0;i<500;++i)rr_campaign_observe(0,1,0);rr_campaign_tick();CHECK(saves==1);
    CHECK(last_path.find("rr2-campaign.state")!=std::string::npos);
    for(int i=0;i<200;++i)rr_campaign_observe(1,0,1);
    for(int i=0;i<200;++i)rr_campaign_observe(0,1,0);rr_campaign_tick();CHECK(saves==1);
    SDL_SetMainReady();SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    CHECK(SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)==0);
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(index>=0);
    auto *joy=SDL_JoystickOpen(index);CHECK(joy);input_map_init_defaults();gamepad_init_players(6);
    int slot=-1;for(int i=0;i<6;++i)if(gamepad_controller_instance(i)==SDL_JoystickInstanceID(joy))slot=i;
    CHECK(slot>=0);g_input_map.p[0].device=INPUT_DEV_GAMEPAD;g_input_map.p[0].controller_slot=slot;
    g_input_map.p[0].pad[GB_B]={GP_BIND_AXIS,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,1};
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);poll();CHECK(loads==0);
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);poll();CHECK(loads==1);
    CHECK(!(gamepad_player_mask(0)&input_button_bit(GB_B)));
    for(int i=0;i<60;++i)poll();CHECK(loads==1);
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);poll();
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);poll();CHECK(loads==2);
    gamepad_shutdown();SDL_JoystickClose(joy);SDL_JoystickDetachVirtual(index);SDL_Quit();
    // A new observation cannot change an already latched raster-frame value.
    rr::ContinuousValue value;CHECK(value.step(0,2.,1.)==2.);
    double corrected=value.step(1,1.8,1.);CHECK(corrected>1.8&&corrected<2.);
    CHECK(value.step(1,2.4,1.)==corrected);
    CHECK(value.step(30,4.,1.)==4.); // real discontinuity resets
    rr::ContinuousValue motion;CHECK(motion.track(0,10.,.2,1.)==10.);
    for(unsigned f=1;f<30;++f)CHECK(std::abs(motion.track(f,10.+.2*f,.2,1.)-(10.+.2*f))<1e-9);
    rr::QuantizedAnchor anchor;CHECK(anchor.step(0,10.,1.)==10.);
    CHECK(anchor.step(1,10.7,1.)==10.); // compatible rounding interval
    CHECK(anchor.step(2,20.,1.)==19.); // true change follows without easing lag
    std::filesystem::remove(root/L".local/saves");std::filesystem::remove(root/L".local");std::filesystem::remove(root);
    std::puts("Campaign: once-per-race autosave, idle/multiplayer rejection, assigned virtual R2 edge and causal coordinate correction passed");return 0;
}
