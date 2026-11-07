#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <cmath>
#include <algorithm>
#include "remix_player.h"
using Microsoft::WRL::ComPtr;
struct Track {std::vector<float> pcm;unsigned rate=44100;std::string title;std::filesystem::path path;};
static std::map<unsigned,Track> tracks;
static Track *current;
static double cursor;
static bool paused,ready;
static unsigned current_cue;
static double music_gain=.20,effects_gain=1.;
static std::string status="Trilha original";
static bool decode(const std::filesystem::path& path,Track& track) {
    ComPtr<IMFSourceReader> reader;
    HRESULT hr=MFCreateSourceReaderFromURL(path.c_str(),nullptr,&reader);
    if(FAILED(hr))return false;
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE);
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE);
    ComPtr<IMFMediaType> format;MFCreateMediaType(&format);
    format->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);format->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_Float);
    format->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2);format->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,44100);
    if(FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,format.Get())))return false;
    while(true) {
        DWORD flags=0;ComPtr<IMFSample> sample;
        if(FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&flags,nullptr,&sample)))return false;
        if(sample) {
            ComPtr<IMFMediaBuffer> buffer;if(FAILED(sample->ConvertToContiguousBuffer(&buffer)))return false;
            BYTE *data=nullptr;DWORD size=0;
            if(FAILED(buffer->Lock(&data,nullptr,&size)))return false;
            if(size%8 || track.pcm.size()+size/4>44100u*2u*60u*12u) {buffer->Unlock();return false;}
            auto *floats=reinterpret_cast<float*>(data);track.pcm.insert(track.pcm.end(),floats,floats+size/4);buffer->Unlock();
        }
        if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;
    }
    return !track.pcm.empty();
}
extern "C" void rr_music_init(const char *game) {
    const char *music=std::getenv("RR_MUSIC_VOLUME"),*effects=std::getenv("RR_EFFECTS_VOLUME");
    rr_music_levels(music?std::atoi(music):20,effects?std::atoi(effects):100);
    const wchar_t *enabled=_wgetenv(L"RR_MUSIC");if(!enabled||wcscmp(enabled,L"dean-harris"))return;
    const wchar_t *root=_wgetenv(L"RR_ROOT");if(!root)return;
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(MFStartup(MF_VERSION,MFSTARTUP_LITE)))return;
    ready=true;
    auto directory=std::filesystem::path(root)/L".local/music/dean-harris";
    // An explicit local pack overrides the portable installer pack per game.
    if(!std::filesystem::exists(directory/(std::string(game)+".tsv")))
        directory=std::filesystem::path(root)/L"music/dean-harris";
    std::ifstream manifest(directory/(std::string(game)+".tsv"));
    std::string line;
    while(std::getline(manifest,line)) {
        auto a=line.find('\t'),b=line.find('\t',a+1);if(a==std::string::npos||b==std::string::npos)continue;
        unsigned cue=(unsigned)std::strtoul(line.substr(0,a).c_str(),nullptr,0);
        Track track;track.title=line.substr(b+1);
        track.path=directory/L"source"/line.substr(a+1,b-a-1);
        if(std::filesystem::exists(track.path))tracks.emplace(cue,std::move(track));
    }
    // RR1 results use cue 9 ($62BA/$8674), while Redwood races use cue 7.
    // Older user packs only map the course. Reuse that file for the results
    // cue without requiring an extra recording or overriding an explicit map.
    if(std::string(game)=="rr1"&&!tracks.count(9)){
        auto redwood=tracks.find(7);
        if(redwood!=tracks.end()){
            Track victory;victory.path=redwood->second.path;victory.title=redwood->second.title;
            tracks.emplace(9,std::move(victory));
        }
    }
    status=tracks.empty()?"Remakes indisponiveis; trilha original":"Dean Harris pronto";
}
extern "C" int rr_music_start(unsigned cue) {
    auto it=tracks.find(cue);if(it==tracks.end()) {rr_music_stop();return 0;}
    if(it->second.pcm.empty()&&!decode(it->second.path,it->second)) {std::fprintf(stderr,"[RR MUSIC] decoder failed cue=%u\n",cue);rr_music_stop();return 0;}
    if(current==&it->second){rr_music_pause(0);return 1;}
    // Keep at most the current and previous decoded song in memory.
    for(auto& pair:tracks)if(&pair.second!=current&&pair.first!=cue)std::vector<float>().swap(pair.second.pcm);
    current=&it->second;current_cue=cue;cursor=0;paused=false;status="Dean Harris: "+current->title;
    std::fprintf(stderr,"[RR MUSIC] playing cue=%u %s\n",cue,current->title.c_str());return 1;
}
extern "C" void rr_music_stop() {
    if(current)std::fprintf(stderr,"[RR MUSIC] stop cue=%u position=%.3f\n",current_cue,rr_music_position());
    current=nullptr;current_cue=0;cursor=0;paused=false;status="Trilha original";
}
extern "C" void rr_music_pause(int value) {
    bool next=value!=0;
    if(next!=paused&&current)std::fprintf(stderr,"[RR MUSIC] %s cue=%u position=%.3f\n",next?"pause":"resume",current_cue,rr_music_position());
    paused=next;
}
extern "C" void rr_music_levels(int music,int effects) {
    music_gain=std::clamp(music,0,100)/100.;effects_gain=std::clamp(effects,0,200)/100.;
}
extern "C" double rr_music_position(){return current?cursor/current->rate:0.;}
extern "C" int rr_music_replacing(){return current!=nullptr;}
extern "C" void rr_music_mix(int16_t *out,size_t frames,double rate) {
    // Native driver contains effects while a remake replaces BGM. With the
    // original soundtrack selected its internal balance remains untouched.
    if(current&&effects_gain!=1.)for(size_t i=0;i<frames*2;++i)
        out[i]=(int16_t)std::clamp((int)std::lround(out[i]*effects_gain),-32768,32767);
    if(!current||paused||rate<=0)return;
    const auto& samples=current->pcm;size_t n=samples.size()/2;
    for(size_t i=0;i<frames;++i) {
        auto first=(size_t)cursor,second=(first+1)%n;double alpha=cursor-first;
        for(int c=0;c<2;++c) {
            double sample=samples[first*2+c]*(1-alpha)+samples[second*2+c]*alpha;
            int mixed=(int)out[i*2+c]+(int)std::lround(sample*32767.0*music_gain);
            out[i*2+c]=(int16_t)std::clamp(mixed,-32768,32767);
        }
        cursor+=current->rate/rate;if(cursor>=n)cursor=std::fmod(cursor,(double)n);
    }
}
extern "C" const char *rr_music_status() {return status.c_str();}
extern "C" void rr_music_shutdown() {current=nullptr;tracks.clear();if(ready)MFShutdown();ready=false;}
