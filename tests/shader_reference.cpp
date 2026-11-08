// Isolated D3D11 comparison fixture; no game, input, or guest writes.
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "gpu_present.h"
#include "png_write.h"
#include <vector>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
int main(int argc,char **argv){
    if(argc!=8)return 2;
    int width=std::atoi(argv[3]),native=std::atoi(argv[4]),ow=std::atoi(argv[5]),oh=std::atoi(argv[6]),frames=std::atoi(argv[7]);
    SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO))return 3;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"direct3d11");
    auto *window=SDL_CreateWindow("Shader comparison QA",0,0,ow,oh,SDL_WINDOW_HIDDEN);
    auto *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);if(!renderer)return 4;
    auto *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,width,224);if(!texture)return 5;
    _putenv_s("RR_SHADER_PATH",argv[1]);
    std::vector<uint32_t> source(width*224),output(ow*oh);
    for(int f=0;f<frames;++f){
        for(int y=0;y<224;++y)for(int x=0;x<width;++x){
            unsigned color=y<76?0xff70c8f0:y<152?0xff205020:0xff101010;
            if(y>=76&&y<152&&std::abs(x-width/2)<(y-60)/2)color=0xff383838;
            if(y>=110&&y<116)color=(f/30)%2?0xffeeeeee:0xff111111;
            if(y>=20&&y<58&&x>=25&&x<100)color=(x&1)?0xfff4c020:0xff101010;
            if(y>=154&&y<210&&x>40&&x<width-40)color=(x/16)%3==0?0xffe02020:(x/16)%3==1?0xff2080e0:0xffe0e0e0;
            source[y*width+x]=color;
        }
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
        SDL_UpdateTexture(texture,nullptr,source.data(),width*4);SDL_RenderCopy(renderer,texture,nullptr,nullptr);
        if(!rr_gpu_present(renderer,source.data(),width,width,224,native,f))return 6;
        if(f==frames-1){
            if(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,output.data(),ow*4))return 7;
            if(png_write_argb(argv[2],output.data(),ow,oh,ow)!=0)return 8;
        }
        SDL_RenderPresent(renderer);
    }
    rr_gpu_shutdown();SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
