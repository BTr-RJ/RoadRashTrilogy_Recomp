#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_1.h>
#include <wrl/client.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_system.h>
#define LIBRA_RUNTIME_D3D11
#include "librashader.h"
#include "librashader_ld.h"
#include "gpu_present.h"
#include "geometry.h"
#include "png_write.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <string>
using Microsoft::WRL::ComPtr;
static ComPtr<ID3D11Device> device;
static ComPtr<ID3D11DeviceContext> context;
static ComPtr<ID3D11DeviceContext1> context1;
static ComPtr<ID3DDeviceContextState> shader_state;
static ComPtr<ID3D11Texture2D> source;
static ComPtr<ID3D11ShaderResourceView> view;
static ComPtr<ID3D11Texture2D> filtered;
static ComPtr<ID3D11RenderTargetView> filtered_target;
static libra_d3d11_filter_chain_t chain;
static bool tried;
static libra_instance_t libra;
static int source_w,source_h;
static int last_native_width;
static float authored_aspect_x,authored_aspect_y;
static bool aspect_overridden;
static bool checked(libra_error_t error) {
    if(!error)return true;
    libra.error_print(error);libra.error_free(&error);return false;
}
extern "C" void rr_gpu_shutdown() {
    if(chain)checked(libra.d3d11_filter_chain_free(&chain));
    filtered_target.Reset();filtered.Reset();view.Reset();source.Reset();shader_state.Reset();context1.Reset();context.Reset();device.Reset();
    tried=false;source_w=source_h=last_native_width=0;
    aspect_overridden=false;
}
static bool init(SDL_Renderer *renderer) {
    tried=true;
    const char *preset=getenv("RR_SHADER_PATH");
    if(!preset||!*preset)return false;
    libra=librashader_load_instance();
    if(!libra.instance_loaded) { std::fprintf(stderr,"[RR SHADER] librashader unavailable, Windows error=%lu\n",GetLastError());return false; }
    device.Attach(SDL_RenderGetD3D11Device(renderer));
    if(!device) {std::fprintf(stderr,"[RR SHADER] Direct3D 11 unavailable\n");return false;}
    device->GetImmediateContext(&context);
    ComPtr<ID3D11Device1> dev1;
    if(FAILED(device.As(&dev1))||FAILED(context.As(&context1))) {std::fprintf(stderr,"[RR SHADER] D3D11.1 context unavailable\n");return false;}
    D3D_FEATURE_LEVEL level=device->GetFeatureLevel();
    UINT flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
    HRESULT state_result=dev1->CreateDeviceContextState(flags,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&shader_state);
    if(FAILED(state_result)) {std::fprintf(stderr,"[RR SHADER] context state failed %08lX\n",state_result);return false;}
    libra_shader_preset_t p=nullptr;
    libra_preset_ctx_t pc=nullptr;
    checked(libra.preset_ctx_create(&pc));
    checked(libra.preset_ctx_set_runtime(&pc,LIBRA_PRESET_CTX_RUNTIME_D3D11));
    libra_preset_opt_t options{};options.version=2;options.original_aspect_uniforms=true;options.frametime_uniforms=true;
    bool ok=checked(libra.preset_create_with_options(preset,&pc,&options,&p));
    if(pc)checked(libra.preset_ctx_free(&pc));
    if(!ok)return false;
    filter_chain_d3d11_opt_t opt{};opt.version=2;
    struct AuthoredParameter {std::string name;float initial;};
    std::vector<AuthoredParameter> authored;
    if(getenv("RR_PRESET_TRACE")) {
        libra_preset_param_list_t parameters{};
        if(checked(libra.preset_get_runtime_params(&p,&parameters))) {
            for(uint64_t i=0;i<parameters.length;i++)authored.push_back({parameters.parameters[i].name,parameters.parameters[i].initial});
            checked(libra.preset_free_runtime_params(parameters));
        }
    }
    ok=checked(libra.d3d11_filter_chain_create(&p,device.Get(),&opt,&chain));
    if(p)checked(libra.preset_free(&p));
    if(ok) {
        if(!checked(libra.d3d11_filter_chain_get_param(&chain,"ASPECT_X",&authored_aspect_x))||
           !checked(libra.d3d11_filter_chain_get_param(&chain,"ASPECT_Y",&authored_aspect_y)))return false;
        // Match Super Monaco 0.16: preserve the resolved artistic settings.
        for(const auto& parameter:authored) {
            float actual=0;
            if(checked(libra.d3d11_filter_chain_get_param(&chain,parameter.name.c_str(),&actual)))
                std::fprintf(stderr,"[RR PRESET ORIGINAL] %s=%.9g authored=%.9g\n",parameter.name.c_str(),actual,parameter.initial);
        }
        std::fprintf(stderr,"[RR SHADER] loaded Slang preset: %s\n",preset);
    }
    return ok;
}
extern "C" int rr_gpu_present(SDL_Renderer *renderer,const uint32_t *pixels,int stride,int width,int height,int native_width,uint64_t frame) {
    if(!tried)init(renderer);
    if(!chain)return 0;
    if(SDL_RenderFlush(renderer)!=0)return 0;
    ComPtr<ID3D11RenderTargetView> target;
    context->OMGetRenderTargets(1,&target,nullptr);
    if(!target)return 0;
    ComPtr<ID3D11Resource> target_resource;target->GetResource(&target_resource);
    ComPtr<ID3D11Texture2D> backbuffer;target_resource.As(&backbuffer);
    if(!backbuffer)return 0;
    D3D11_TEXTURE2D_DESC back_desc{},filtered_desc{};backbuffer->GetDesc(&back_desc);
    if(filtered)filtered->GetDesc(&filtered_desc);
    if(!filtered||filtered_desc.Width!=back_desc.Width||filtered_desc.Height!=back_desc.Height||filtered_desc.Format!=back_desc.Format) {
        filtered_target.Reset();filtered.Reset();
        auto desc=back_desc;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.MiscFlags=0;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&filtered))||
           FAILED(device->CreateRenderTargetView(filtered.Get(),nullptr,&filtered_target)))return 0;
    }
    if(width!=source_w||height!=source_h) {
        last_native_width=0;
        view.Reset();source.Reset();
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&source))||FAILED(device->CreateShaderResourceView(source.Get(),nullptr,&view)))return 0;
        source_w=width;source_h=height;
    }
    if(last_native_width!=native_width || frame==0)last_native_width=0;
    const float aspect=(float)rr_display_aspect(width,native_width);
    // Super Monaco 0.16 display integration for expanded canvases. Native
    // 4:3 keeps the previous Road Rash preset values, without setters.
    if(width>native_width){
        if(!checked(libra.d3d11_filter_chain_set_param(&chain,"ASPECT_X",aspect))||
           !checked(libra.d3d11_filter_chain_set_param(&chain,"ASPECT_Y",1.f)))return 0;
        aspect_overridden=true;
    }else if(aspect_overridden){
        if(!checked(libra.d3d11_filter_chain_set_param(&chain,"ASPECT_X",authored_aspect_x))||
           !checked(libra.d3d11_filter_chain_set_param(&chain,"ASPECT_Y",authored_aspect_y)))return 0;
        aspect_overridden=false;
    }
    if(!last_native_width)std::fprintf(stderr,"[RR SHADER] source=%dx%d native=%d aspect=%.7f\n",width,height,native_width,aspect);
    last_native_width=native_width;
    context->UpdateSubresource(source.Get(),0,nullptr,pixels,stride*4,0);
    int output_w=0,output_h=0;SDL_GetRendererOutputSize(renderer,&output_w,&output_h);
    if(output_w<1||output_h<1)return 0;
    ComPtr<ID3DDeviceContextState> previous;
    context1->SwapDeviceContextState(shader_state.Get(),&previous);
    float clear[4]={0,0,0,1};context->ClearRenderTargetView(filtered_target.Get(),clear);
    libra_viewport_t viewport{0,0,(uint32_t)output_w,(uint32_t)output_h};
    frame_d3d11_opt_t options{};options.version=2;options.frame_direction=1;
    options.total_subframes=options.current_subframe=1;
    options.aspect_ratio=aspect;
    options.frames_per_second=59.94006f;options.frametime_delta=17;
    bool ok=checked(libra.d3d11_filter_chain_frame(&chain,context.Get(),(size_t)frame,view.Get(),filtered_target.Get(),&viewport,nullptr,&options));
    // Commit only successful shader frames. Failed presets leave SDL's
    // already rendered image visible, instead of a cleared black screen.
    if(ok)context->CopyResource(backbuffer.Get(),filtered.Get());
    // Restore every D3D binding so SDL's cached state remains valid next frame.
    context1->SwapDeviceContextState(previous.Get(),nullptr);
    if(const char *capture=getenv("RR_GPU_CAPTURE")) {
        static bool captured=false;
        const char *at=getenv("RR_GPU_CAPTURE_FRAME");
        if(!captured&&frame>=(at?(uint64_t)strtoull(at,nullptr,10):120)) {
            ComPtr<ID3D11Resource> resource;target->GetResource(&resource);
            ComPtr<ID3D11Texture2D> back;resource.As(&back);D3D11_TEXTURE2D_DESC d{};back->GetDesc(&d);
            d.BindFlags=0;d.MiscFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging;
            if(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging))) {
                context->CopyResource(staging.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE m{};
                if(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m))) {
                    png_write_argb(capture,(const uint32_t*)m.pData,d.Width,d.Height,m.RowPitch/4);context->Unmap(staging.Get(),0);captured=true;
                    std::fprintf(stderr,"[RR SHADER] captured %ux%u format=%u\n",d.Width,d.Height,d.Format);
                }
            }
        }
    }
    return ok?1:0;
}
// Capture the actual presented backbuffer for all filters, including LUT/raw.
// Independent of shader initialization, so a silent fallback is visible in QA.
extern "C" void rr_present_capture(SDL_Renderer *renderer,uint64_t frame) {
    const char *directory=getenv("RR_PRESENT_CAPTURE_DIR");if(!directory)return;
    const char *at=getenv("RR_PRESENT_CAPTURE_FRAME");
    static bool captured=false;
    if(captured||frame!=(at?(uint64_t)strtoull(at,nullptr,10):2340))return;
    SDL_RenderFlush(renderer);
    ComPtr<ID3D11Device> d;d.Attach(SDL_RenderGetD3D11Device(renderer));if(!d)return;
    ComPtr<ID3D11DeviceContext> c;d->GetImmediateContext(&c);
    ComPtr<ID3D11RenderTargetView> rt;c->OMGetRenderTargets(1,&rt,nullptr);if(!rt)return;
    ComPtr<ID3D11Resource> resource;rt->GetResource(&resource);
    ComPtr<ID3D11Texture2D> back;resource.As(&back);if(!back)return;
    D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);
    desc.BindFlags=0;desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;if(FAILED(d->CreateTexture2D(&desc,nullptr,&staging)))return;
    c->CopyResource(staging.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return;
    auto path=std::filesystem::path(directory)/"present.png";
    png_write_argb(path.string().c_str(),(uint32_t*)mapped.pData,desc.Width,desc.Height,mapped.RowPitch/4);
    c->Unmap(staging.Get(),0);captured=true;
    int lw,lh;SDL_RenderGetLogicalSize(renderer,&lw,&lh);
    std::fprintf(stderr,"[RR PRESENT] captured %ux%u logical=%dx%d shader=%s\n",desc.Width,desc.Height,lw,lh,chain?"active":"none");
}
