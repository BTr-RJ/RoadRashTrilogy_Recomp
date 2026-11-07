#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "music/remix_player.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
namespace fs=std::filesystem;
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"MUSIC FAIL %d: %s\n",__LINE__,#c);return 1;}}while(0)
static void put(std::ofstream& f,unsigned v,int n){for(int i=0;i<n;++i)f.put((char)(v>>(i*8)));}
int wmain(int argc,wchar_t **argv){
    if(argc==2){
        // Optional real installer-pack smoke check, without ROMs or game code.
        fs::path installed=argv[1];unsigned count=0,audible=0;
        _wputenv_s(L"RR_ROOT",installed.c_str());_putenv_s("RR_MUSIC","dean-harris");
        for(auto game:{"rr1","rr2","rr3"}){
            rr_music_init(game);
            std::ifstream manifest(installed/L"music/dean-harris"/(std::string(game)+".tsv"));
            CHECK(manifest.good());std::string line;unsigned game_count=0;
            while(std::getline(manifest,line)){
                CHECK(rr_music_start((unsigned)std::strtoul(line.c_str(),nullptr,10)));
                std::vector<int16_t> samples(176400,0);rr_music_mix(samples.data(),88200,44100);
                CHECK(rr_music_position()>1.999);
                // Introductory silence is valid audio; require audible output
                // across the pack, not in every track's opening 100 ms.
                if(std::any_of(samples.begin(),samples.end(),[](auto value){return value!=0;}))++audible;
                ++count;++game_count;
            }
            CHECK(game_count);rr_music_shutdown();
        }
        CHECK(audible);std::printf("Portable remake pack: %u cues decoded and mixed from installation root (%u audible intros)\n",count,audible);return 0;
    }
    auto root=fs::temp_directory_path()/(L"rr-music-"+std::to_wstring(GetCurrentProcessId()));
    auto folder=root/L".local/music/dean-harris";fs::create_directories(folder/L"source");
    {
        std::ofstream f(folder/L"source/test.wav",std::ios::binary);
        f.write("RIFF",4);put(f,36+44100*4,4);f.write("WAVEfmt ",8);put(f,16,4);put(f,1,2);put(f,2,2);
        put(f,44100,4);put(f,44100*4,4);put(f,4,2);put(f,16,2);f.write("data",4);put(f,44100*4,4);
        for(int i=0;i<44100;++i){int16_t sample=(int16_t)(std::sin(i*.05)*10000);put(f,(uint16_t)sample,2);put(f,(uint16_t)sample,2);}
        std::ofstream manifest(folder/L"test.tsv");manifest<<"1\ttest.wav\tSynthetic regression track\n";
    }
    _wputenv_s(L"RR_ROOT",root.c_str());_putenv_s("RR_MUSIC","dean-harris");rr_music_init("test");
    CHECK(rr_music_start(1));rr_music_levels(20,100);
    std::vector<int16_t> buffer(4410*2,0);rr_music_mix(buffer.data(),4410,44100);
    CHECK(rr_music_position()>.099&&rr_music_position()<.101);
    double held=rr_music_position();rr_music_pause(1);std::fill(buffer.begin(),buffer.end(),1234);
    rr_music_mix(buffer.data(),4410,44100);CHECK(rr_music_position()==held);
    for(auto x:buffer)CHECK(x==1234);
    rr_music_pause(0);rr_music_mix(buffer.data(),4410,44100);CHECK(rr_music_position()>held+.099);
    held=rr_music_position();CHECK(rr_music_start(1));CHECK(rr_music_position()==held);
    rr_music_levels(0,150);std::fill(buffer.begin(),buffer.end(),1000);rr_music_mix(buffer.data(),4410,44100);
    for(auto x:buffer)CHECK(x==1500);
    rr_music_levels(100,200);std::fill(buffer.begin(),buffer.end(),32767);rr_music_mix(buffer.data(),4410,44100);
    for(auto x:buffer)CHECK(x>=0); // no integer wrap on loud effects + music
    CHECK(!rr_music_start(999));CHECK(rr_music_position()==0);
    std::fill(buffer.begin(),buffer.end(),1000);rr_music_mix(buffer.data(),4410,44100);
    for(auto x:buffer)CHECK(x==1000); // fallback retains the original mix
    rr_music_shutdown();
    // Old local packs have Redwood (cue 7), but no finish-screen entry (cue 9).
    {std::ofstream f(folder/L"rr1.tsv");f<<"7\ttest.wav\tRedwood Forest\n";}
    rr_music_init("rr1");CHECK(rr_music_start(9));
    CHECK(std::string(rr_music_status()).find("Redwood Forest")!=std::string::npos);
    std::fill(buffer.begin(),buffer.end(),0);rr_music_mix(buffer.data(),4410,44100);
    CHECK(rr_music_position()>.099);
    CHECK(rr_music_start(7));CHECK(rr_music_position()==0); // a different scene cue restarts
    rr_music_shutdown();
    {std::ofstream f(folder/L"rr2.tsv");f<<"7\ttest.wav\tNot an RR1 pack\n";}
    rr_music_init("rr2");CHECK(!rr_music_start(9));rr_music_shutdown();
    // A fresh PC needs only the public music tree, with per-game local override.
    auto bundled=root/L"music/dean-harris";fs::create_directories(bundled/L"source");
    fs::copy_file(folder/L"source/test.wav",bundled/L"source/test.wav");
    {std::ofstream f(bundled/L"rr3.tsv");f<<"1\ttest.wav\tBundled portable track\n";}
    {std::ofstream f(bundled/L"rr1.tsv");f<<"7\ttest.wav\tBundled fallback\n";}
    rr_music_init("rr3");CHECK(rr_music_start(1));
    CHECK(std::string(rr_music_status()).find("Bundled portable track")!=std::string::npos);
    std::fill(buffer.begin(),buffer.end(),0);rr_music_mix(buffer.data(),4410,44100);CHECK(rr_music_position()>.099);
    rr_music_shutdown();rr_music_init("rr1");CHECK(rr_music_start(7));
    CHECK(std::string(rr_music_status()).find("Redwood Forest")!=std::string::npos);rr_music_shutdown();
    fs::remove(bundled/L"rr1.tsv");fs::remove(bundled/L"rr3.tsv");fs::remove(bundled/L"source/test.wav");
    fs::remove(bundled/L"source");fs::remove(bundled);fs::remove(root/L"music");
    fs::remove(folder/L"source/test.wav");fs::remove(folder/L"test.tsv");fs::remove(folder/L"rr1.tsv");fs::remove(folder/L"rr2.tsv");
    fs::remove(folder/L"source");fs::remove(folder);fs::remove(root/L".local/music");fs::remove(root/L".local");fs::remove(root);
    std::puts("Music: real MF decode, pause/resume cursor, repeated cue, separate levels, clipping and original fallback passed");return 0;
}
