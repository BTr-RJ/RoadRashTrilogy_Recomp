#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include "campaign_state.h"
#include "remix_player.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "glue.h"
#include "input_map.h"
#include "input_script.h"
#include "gamepad.h"
#include "sim_step.h"
#include "audio/event_queue.h"
#include "genesis_machine.h"
size_t glue_rb_sched_save(void *,size_t);
int glue_rb_sched_load(const void *,size_t);
void rr_state_resume(void);
int runner_save_state_file(const char *);
int runner_load_state_file(const char *);
}
static std::string save_path;
static unsigned race_ticks,idle_ticks;
static bool armed,pending,trigger_held;
static bool enabled;
char rr_campaign_build_id[65];
extern "C" const char *rr_campaign_unavailable(){return *rr_campaign_build_id?nullptr:"Cannot verify this executable's state format";}
static void build_identity(){
    rr_campaign_build_id[0]=0;HCRYPTPROV provider=0;HCRYPTHASH hash=0;
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return;
    if(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);
        std::ifstream f(n&&n<32768?std::filesystem::path(exe):std::filesystem::path(),std::ios::binary);
        bool ok=f.good();char buffer[65536];
        while(ok&&f){f.read(buffer,sizeof buffer);if(f.gcount())ok=CryptHashData(hash,(const BYTE*)buffer,(DWORD)f.gcount(),0)!=0;}
        BYTE digest[32]={};DWORD size=sizeof digest;
        ok=ok&&f.eof()&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32;
        if(ok)for(unsigned i=0;i<32;++i)std::sprintf(rr_campaign_build_id+i*2,"%02x",digest[i]);
        CryptDestroyHash(hash);
    }
    CryptReleaseContext(provider,0);
}
static bool participated;
static unsigned message_ticks;
static std::string message;
extern "C" void rr_campaign_notice(const char *text){
    message=text?text:"";
    if(message.find("Load rejected")==0)message="No compatible saved state available";
    if(message.find("Save failed")==0)message="Save failed; previous state kept";
    message_ticks=240;
}
extern "C" const char *rr_campaign_message(){return message_ticks?message.c_str():"";}
struct CampaignState {
    uint32_t version,scheduler_bytes,audio_bytes,machine_bytes,sim_tick;
    GenesisSimInput input;
    RRMusicState music;
    uint64_t irq_epoch;
    unsigned music_cue;
};
extern "C" void rr_campaign_init(const char *game){
    build_identity();
    const wchar_t *root=_wgetenv(L"RR_ROOT");
    std::filesystem::path path;
    if(root)path=std::filesystem::path(root)/L".local/saves";
    else {wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);path=std::filesystem::path(exe).parent_path()/L"saves";}
    path/=std::string(game)+"-campaign.state";
    auto utf=path.u8string();save_path.assign(utf.begin(),utf.end());
    enabled=!input_script_active()||std::getenv("RR_CAMPAIGN_QA");
    race_ticks=idle_ticks=0;armed=pending=trigger_held=participated=false;
}
extern "C" void rr_campaign_observe(int racing,int campaign,int player_input){
    if(message_ticks)--message_ticks;
    if(!enabled)return;
    if(!campaign){race_ticks=idle_ticks=0;armed=pending=participated=false;return;}
    if(racing){participated|=player_input!=0;if(race_ticks<180)++race_ticks;if(race_ticks>=180&&participated)armed=true;idle_ticks=0;}
    else if(armed&&++idle_ticks>=90){armed=false;race_ticks=0;participated=false;pending=true;}
}
extern "C" void rr_campaign_tick(){
    if(!pending||save_path.empty())return;
    pending=false;
    std::error_code error;std::filesystem::create_directories(std::filesystem::u8path(save_path).parent_path(),error);
    if(error){std::fprintf(stderr,"[RR CAMPAIGN] cannot create save directory: %s\n",error.message().c_str());return;}
    std::fprintf(stderr,"[RR CAMPAIGN] race completed; autosave requested\n");
    runner_save_state_file(save_path.c_str());
}
extern "C" int rr_campaign_load(){
    if(save_path.empty())return 0;
    return runner_load_state_file(save_path.c_str());
}
extern "C" void rr_campaign_input(){
    int slot=g_input_map.p[0].controller_slot;
    auto *pad=SDL_GameControllerFromInstanceID(gamepad_controller_instance(slot));
    int value=pad&&(g_input_map.p[0].device&INPUT_DEV_GAMEPAD)?SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT):0;
    if(value<=8192)trigger_held=false;
    if(value>=16384&&!trigger_held){trigger_held=true;rr_campaign_load();}
}
extern "C" size_t rr_campaign_state_size(){return sizeof(CampaignState)+glue_rb_sched_save(nullptr,0)+audio_event_state_size()+machine_rb_save(nullptr,0);}
extern "C" int rr_campaign_state_save(void *data,size_t size){
    if(!data||size!=rr_campaign_state_size()||!glue_state_boundary_ready())return 0;
    CampaignState h{};h.version=1;h.scheduler_bytes=(uint32_t)glue_rb_sched_save(nullptr,0);
    h.audio_bytes=(uint32_t)audio_event_state_size();h.sim_tick=genesis_sim_tick_count();
    h.machine_bytes=(uint32_t)machine_rb_save(nullptr,0);
    h.input=*genesis_sim_input();rr_music_capture(&h.music);
    rr_adapter_state(&h.irq_epoch,&h.music_cue,0);
    auto *p=(uint8_t*)data;std::memcpy(p,&h,sizeof h);p+=sizeof h;
    if(glue_rb_sched_save(p,h.scheduler_bytes)!=h.scheduler_bytes)return 0;p+=h.scheduler_bytes;
    if(!audio_event_state_save(p,h.audio_bytes))return 0;p+=h.audio_bytes;
    return machine_rb_save(p,h.machine_bytes)==h.machine_bytes;
}
extern "C" int rr_campaign_state_load(const void *data,size_t size,int apply){
    if(!data||size!=rr_campaign_state_size())return 0;
    CampaignState h;std::memcpy(&h,data,sizeof h);
    if(h.version!=1||h.scheduler_bytes!=glue_rb_sched_save(nullptr,0)||h.audio_bytes!=audio_event_state_size()||h.machine_bytes!=machine_rb_save(nullptr,0)||
       !rr_music_restore(&h.music,0))return 0;
    auto *p=(const uint8_t*)data+sizeof h;
    if(!audio_event_state_load(p+h.scheduler_bytes,h.audio_bytes,0))return 0;
    if(!apply)return 1;
    if(!glue_rb_sched_load(p,h.scheduler_bytes)||!audio_event_state_load(p+h.scheduler_bytes,h.audio_bytes,1))return 0;
    if(!machine_rb_load(p+h.scheduler_bytes+h.audio_bytes,h.machine_bytes))return 0;
    genesis_sim_input_restore(&h.input);genesis_sim_set_tick_count(h.sim_tick);
    rr_adapter_state(&h.irq_epoch,&h.music_cue,1);
    rr_music_restore(&h.music,1);rr_state_resume();
    pending=armed=participated=false;race_ticks=idle_ticks=0;
    return 1;
}
