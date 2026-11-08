#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
#include "local_achievements.h"
static const char *names[]={"Primeira largada","Motor aquecido","Ritmo de corrida","Um minuto na estrada","Cinco minutos na estrada","De volta ao asfalto"};
static uint32_t unlocked,driving,sessions;
static bool panel,persistent,was_racing;
static std::filesystem::path path;
static std::string toast;
static uint32_t toast_ticks;
static unsigned quiet_ticks;
static void save() {
    if(!persistent||path.empty())return;
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
    auto temp=path;temp+=L".tmp";
    {std::ofstream file(temp,std::ios::binary);file<<"RRLOCAL1 "<<unlocked<<' '<<driving<<' '<<sessions<<'\n';if(!file)return;}
    MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
extern "C" void rr_local_init(const char *game,int persist) {
    unlocked=driving=sessions=toast_ticks=0;panel=was_racing=false;toast.clear();persistent=persist!=0;
    const wchar_t *root=_wgetenv(L"RR_ROOT");
    if(root)path=std::filesystem::path(root)/L".local/achievements"/(std::string(game)+".dat");
    else {wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);path=std::filesystem::path(exe).parent_path()/L"saves"/(std::string(game)+"-achievements.dat");}
    if(!persistent)return;
    std::ifstream file(path);std::string magic;uint32_t bits=0,ticks=0,runs=0;
    if(file>>magic>>bits>>ticks>>runs&&magic=="RRLOCAL1"&&bits<64&&ticks<1000000000&&runs<1000000) {unlocked=bits;driving=ticks;sessions=runs;}
}
static void award(unsigned id) {
    if(unlocked&(1u<<id))return;unlocked|=1u<<id;toast=names[id];toast_ticks=360;save();
    std::fprintf(stderr,"[RR LOCAL ACHIEVEMENT] %u %s\n",id,names[id]);
}
extern "C" void rr_local_frame(int racing,unsigned speed,unsigned,int paused) {
    if(quiet_ticks)--quiet_ticks;
    if(toast_ticks&&!--toast_ticks)toast.clear();
    if(racing&&!was_racing){++sessions;save();}was_racing=racing!=0;
    if(!racing||paused||speed>0x7fff)return;
    if(speed>0) {award(0);++driving;}
    if(speed>=0x1000)award(1);if(speed>=0x2000)award(2);
    if(driving>=3600)award(3);if(driving>=18000)award(4);
    if(sessions>=2&&speed>0)award(5);
    if(driving&&driving%1800==0)save();
}
extern "C" void rr_local_toggle(){panel=!panel;}
extern "C" void rr_local_state_loaded(){panel=false;toast.clear();toast_ticks=0;quiet_ticks=360;}
extern "C" int rr_local_panel(){return panel;}
extern "C" const char *rr_local_title(unsigned i){return i<6?names[i]:"";}
extern "C" int rr_local_unlocked(unsigned i){return i<6&&(unlocked&(1u<<i));}
extern "C" const char *rr_local_toast(){return quiet_ticks?"":toast.c_str();}
extern "C" unsigned rr_local_count(){return 6;}
