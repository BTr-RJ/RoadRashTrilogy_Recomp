#define NOMINMAX
#include "controls.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <SDL.h>
#include <array>
#include <string>
#include <algorithm>
extern "C" {
#include "input_map.h"
#include "app_config.h"
#include "gamepad.h"
}
namespace {
HINSTANCE app;HFONT font;HWND panel,owner,player_box,device_box,pad_box,type_box,zone_box,analog_box,status;
std::array<HWND,GB_COUNT> keys,pads;
std::filesystem::path config;
InputMap previous;
int player=0,capture=-1;bool capture_pad=false,armed=false;
ULONGLONG deadline;
HBRUSH background;
float dpi=1;
const wchar_t *names[]={L"Up",L"Down",L"Left",L"Right",L"A",L"B",L"C",L"Start",L"X",L"Y",L"Z",L"Mode"};
std::wstring widen(const char *s){if(!s)return L"";int n=MultiByteToWideChar(CP_UTF8,0,s,-1,nullptr,0);std::wstring out(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s,-1,out.data(),n);if(n)out.pop_back();return out;}
std::wstring pad_name(const GamepadBind& b){
    if(b.kind==GP_BIND_BUTTON){
        const wchar_t *labels[]={L"A / Cross",L"B / Circle",L"X / Square",L"Y / Triangle",L"Back / View",L"Guide",L"Start / Menu",L"Left stick click",L"Right stick click",L"Left shoulder",L"Right shoulder",L"D-pad up",L"D-pad down",L"D-pad left",L"D-pad right"};
        if(b.code>=0&&b.code<15)return labels[b.code];
        return widen(SDL_GameControllerGetStringForButton((SDL_GameControllerButton)b.code));
    }
    if(b.kind==GP_BIND_AXIS)return widen(SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)b.code))+(b.axis_dir<0?L" -":L" +");
    return L"Unbound";
}
void hint(const std::wstring& s){SetWindowTextW(status,s.c_str());}
void refresh(){
    const auto& p=g_input_map.p[player];
    SendMessageW(player_box,CB_SETCURSEL,player,0);SendMessageW(device_box,CB_SETCURSEL,p.device,0);
    SendMessageW(pad_box,CB_SETCURSEL,std::clamp(p.controller_slot,0,1),0);
    SendMessageW(type_box,CB_SETCURSEL,p.pad_type,0);SendMessageW(zone_box,TBM_SETPOS,TRUE,p.deadzone_pct);
    SendMessageW(analog_box,BM_SETCHECK,p.analog_dpad?BST_CHECKED:BST_UNCHECKED,0);
    for(int b=0;b<GB_COUNT;++b){
        std::wstring k=p.key[b]>0?widen(SDL_GetScancodeName((SDL_Scancode)p.key[b])):L"Unbound";
        SetWindowTextW(keys[b],k.c_str());auto name=pad_name(p.pad[b]);SetWindowTextW(pads[b],name.c_str());
        EnableWindow(keys[b],p.device&INPUT_DEV_KEYBOARD);EnableWindow(pads[b],p.device&INPUT_DEV_GAMEPAD);
    }
}
void cancel_capture(){capture=-1;refresh();hint(L"Choose a binding. Escape cancels capture; Delete clears it. Changes apply only after Save.");}
void key_binding(int code){
    auto& p=g_input_map.p[player];int old=p.key[capture];
    if(code)for(int b=0;b<GB_COUNT;++b)if(b!=capture&&p.key[b]==code)p.key[b]=old;
    p.key[capture]=code;cancel_capture();
}
void pad_binding(GamepadBind value){
    auto& p=g_input_map.p[player];auto old=p.pad[capture];
    for(int b=0;b<GB_COUNT;++b)if(b!=capture&&value.kind&&p.pad[b].kind==value.kind&&p.pad[b].code==value.code&&p.pad[b].axis_dir==value.axis_dir)p.pad[b]=old;
    p.pad[capture]=value;cancel_capture();
}
int scancode(WPARAM vk,LPARAM raw){
    if(vk>='A'&&vk<='Z')return SDL_SCANCODE_A+(int)vk-'A';
    if(vk>='1'&&vk<='9')return SDL_SCANCODE_1+(int)vk-'1';
    if(vk=='0')return SDL_SCANCODE_0;
    switch(vk){
    case VK_UP:return SDL_SCANCODE_UP;case VK_DOWN:return SDL_SCANCODE_DOWN;case VK_LEFT:return SDL_SCANCODE_LEFT;case VK_RIGHT:return SDL_SCANCODE_RIGHT;
    case VK_RETURN:return (raw&(1<<24))?SDL_SCANCODE_KP_ENTER:SDL_SCANCODE_RETURN;
    case VK_SPACE:return SDL_SCANCODE_SPACE;case VK_BACK:return SDL_SCANCODE_BACKSPACE;
    case VK_SHIFT:return ((raw>>16)&255)==0x36?SDL_SCANCODE_RSHIFT:SDL_SCANCODE_LSHIFT;
    case VK_CONTROL:return raw&(1<<24)?SDL_SCANCODE_RCTRL:SDL_SCANCODE_LCTRL;
    case VK_MENU:return raw&(1<<24)?SDL_SCANCODE_RALT:SDL_SCANCODE_LALT;
    case VK_OEM_COMMA:return SDL_SCANCODE_COMMA;case VK_OEM_PERIOD:return SDL_SCANCODE_PERIOD;case VK_OEM_2:return SDL_SCANCODE_SLASH;
    case VK_HOME:return SDL_SCANCODE_HOME;case VK_END:return SDL_SCANCODE_END;case VK_PRIOR:return SDL_SCANCODE_PAGEUP;case VK_NEXT:return SDL_SCANCODE_PAGEDOWN;
    }
    if(vk>=VK_NUMPAD0&&vk<=VK_NUMPAD9){int n=(int)vk-VK_NUMPAD0;return n?SDL_SCANCODE_KP_1+n-1:SDL_SCANCODE_KP_0;}
    return 0;
}
void update_options(){auto& p=g_input_map.p[player];p.device=(int)SendMessageW(device_box,CB_GETCURSEL,0,0);p.controller_slot=(int)SendMessageW(pad_box,CB_GETCURSEL,0,0);p.pad_type=(int)SendMessageW(type_box,CB_GETCURSEL,0,0);p.deadzone_pct=(int)SendMessageW(zone_box,TBM_GETPOS,0,0);p.analog_dpad=SendMessageW(analog_box,BM_GETCHECK,0,0)==BST_CHECKED;}
bool save(){
    const auto& a=g_input_map.p[0];const auto& b=g_input_map.p[1];
    if((a.device&2)&&(b.device&2)&&a.controller_slot==b.controller_slot){hint(L"Player 1 and Player 2 need different gamepads. Select a different gamepad or use keyboard only for one player.");return false;}
    auto temporary=config;temporary+=L".tmp";
    if(!app_config_save(temporary.u8string().c_str())||!MoveFileExW(temporary.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){hint(L"Could not save controls. Check free space and folder permissions.");return false;}
    previous=g_input_map;return true;
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(m==WM_CREATE){
        font=CreateFontW(-(int)(16*dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Barlow");
        auto add=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,(int)(x*dpi),(int)(y*dpi),(int)(width*dpi),(int)(height*dpi),w,(HMENU)(INT_PTR)id,app,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;};
        add(L"STATIC",L"Player",0,24,18,140,24,0);player_box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,24,44,140,180,10);
        for(auto s:{L"Player 1",L"Player 2"})SendMessageW(player_box,CB_ADDSTRING,0,(LPARAM)s);
        add(L"STATIC",L"Input source",0,184,18,230,24,0);device_box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,184,44,240,180,11);
        for(auto s:{L"Disabled",L"Keyboard only",L"Gamepad only",L"Keyboard + gamepad"})SendMessageW(device_box,CB_ADDSTRING,0,(LPARAM)s);
        add(L"STATIC",L"Assigned gamepad",0,448,18,260,24,0);pad_box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,448,44,290,180,12);
        for(auto s:{L"Gamepad 1",L"Gamepad 2"})SendMessageW(pad_box,CB_ADDSTRING,0,(LPARAM)s);
        add(L"STATIC",L"Genesis button",0,24,96,150,24,0);add(L"STATIC",L"Keyboard",0,184,96,240,24,0);add(L"STATIC",L"Gamepad",0,448,96,290,24,0);
        for(int b=0;b<GB_COUNT;++b){add(L"STATIC",names[b],0,24,126+b*27,140,25,0);keys[b]=add(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,184,122+b*27,240,25,100+b);pads[b]=add(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,448,122+b*27,290,25,200+b);}
        type_box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,24,464,170,100,13);
        for(auto s:{L"3-button pad",L"6-button pad"})SendMessageW(type_box,CB_ADDSTRING,0,(LPARAM)s);
        analog_box=add(L"BUTTON",L"Left stick also steers",BS_AUTOCHECKBOX|WS_TABSTOP,216,466,240,28,14);
        SetWindowTheme(analog_box,L"",L"");
        add(L"STATIC",L"Deadzone",0,478,456,240,22,0);zone_box=add(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,474,478,264,30,15);SendMessageW(zone_box,TBM_SETRANGE,TRUE,MAKELONG(5,90));
        status=add(L"STATIC",L"",0,24,518,714,48,0);
        add(L"STATIC",L"Player 2 uses the original split screen in Road Rash II / 3. F-keys, Tab and Escape are reserved.",0,24,570,714,36,0);
        add(L"BUTTON",L"Reset player",BS_PUSHBUTTON|WS_TABSTOP,24,612,150,34,20);add(L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,470,612,124,34,IDCANCEL);add(L"BUTTON",L"Save controls",BS_DEFPUSHBUTTON|WS_TABSTOP,610,612,128,34,IDOK);
        SetTimer(w,1,30,nullptr);refresh();cancel_capture();return 0;
    }
    if(m==WM_COMMAND){int id=LOWORD(wp);
        if(id==10&&HIWORD(wp)==CBN_SELCHANGE){player=(int)SendMessageW(player_box,CB_GETCURSEL,0,0);cancel_capture();return 0;}
        if((id>=11&&id<=13&&HIWORD(wp)==CBN_SELCHANGE)||(id==14&&HIWORD(wp)==BN_CLICKED)){update_options();cancel_capture();return 0;}
        if((id>=100&&id<100+GB_COUNT)||(id>=200&&id<200+GB_COUNT)){
            capture_pad=id>=200;capture=id-(capture_pad?200:100);deadline=GetTickCount64()+15000;armed=false;
            SetWindowTextW((capture_pad?pads:keys)[capture],L"Waiting for input...");hint(capture_pad?L"Release the gamepad, then press a button or move an axis. Escape cancels; Delete clears.":L"Press a key. Escape cancels; Delete clears. Duplicate bindings within this player are swapped.");return 0;
        }
        if(id==20){InputMap current=g_input_map;input_map_init_defaults();PlayerInput defaults=g_input_map.p[player];g_input_map=current;g_input_map.p[player]=defaults;cancel_capture();return 0;}
        if(id==IDOK){update_options();if(save())DestroyWindow(w);return 0;}
        if(id==IDCANCEL){g_input_map=previous;DestroyWindow(w);return 0;}
    }
    if(m==WM_HSCROLL){update_options();return 0;}
    if(m==WM_TIMER){rr_controls_poll();if(capture>=0&&GetTickCount64()>deadline){cancel_capture();hint(L"Input capture timed out. Choose a binding to try again.");}return 0;}
    if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX||m==WM_CTLCOLORBTN){SetTextColor((HDC)wp,RGB(232,234,233));SetBkColor((HDC)wp,RGB(26,29,31));return(LRESULT)background;}
    if(m==WM_CLOSE){g_input_map=previous;DestroyWindow(w);return 0;}
    if(m==WM_DESTROY){panel=nullptr;capture=-1;DeleteObject(font);font=nullptr;EnableWindow(owner,TRUE);if(!GetEnvironmentVariableW(L"RR_QA_CAPTURE",nullptr,0))SetForegroundWindow(owner);return 0;}
    return DefWindowProcW(w,m,wp,lp);
}
}
void rr_controls_init(HINSTANCE instance,HFONT body,const std::filesystem::path& path){
    app=instance;font=body;config=path;std::filesystem::create_directories(config.parent_path());
    input_map_init_defaults();app_config_defaults();app_config_load(config.u8string().c_str());
    for(int p=0;p<2;++p){auto& a=g_input_map.p[p];a.device=std::clamp(a.device,0,3);a.pad_type=std::clamp(a.pad_type,0,1);a.deadzone_pct=std::clamp(a.deadzone_pct,5,90);a.controller_slot=std::clamp(a.controller_slot,0,1);for(auto& key:a.key)if(key<0||key>=SDL_NUM_SCANCODES)key=0;for(auto& binding:a.pad)if(binding.kind<0||binding.kind>2||(binding.kind==1&&(binding.code<0||binding.code>=SDL_CONTROLLER_BUTTON_MAX))||(binding.kind==2&&(binding.code<0||binding.code>=SDL_CONTROLLER_AXIS_MAX)))binding={};}
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS);gamepad_init();
    background=CreateSolidBrush(RGB(26,29,31));WNDCLASSW klass{};klass.hInstance=app;klass.hCursor=LoadCursor(nullptr,IDC_ARROW);klass.hbrBackground=background;klass.lpszClassName=L"RRControls";klass.lpfnWndProc=proc;RegisterClassW(&klass);
}
void rr_controls_show(HWND parent){
    if(panel){SetForegroundWindow(panel);return;}
    owner=parent;previous=g_input_map;player=0;dpi=GetDpiForWindow(parent)/96.f;RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);dpi=std::min(dpi,(work.bottom-work.top)/700.f);
    RECT r{};GetWindowRect(parent,&r);int width=(int)(782*dpi),height=(int)(700*dpi);EnableWindow(parent,FALSE);
    panel=CreateWindowExW(WS_EX_DLGMODALFRAME|(GetEnvironmentVariableW(L"RR_QA_CAPTURE",nullptr,0)?WS_EX_NOACTIVATE:0),L"RRControls",L"Controls - keyboard and gamepad",WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,r.left+(r.right-r.left-width)/2,std::max(work.top,r.top+(r.bottom-r.top-height)/2),width,height,parent,nullptr,app,nullptr);
}
bool rr_controls_message(MSG& message){
    if(!panel)return false;
    if(capture>=0&&message.message==WM_KEYDOWN){
        if(message.wParam==VK_ESCAPE){cancel_capture();return true;}
        if(message.wParam==VK_DELETE){if(capture_pad)pad_binding({});else key_binding(0);return true;}
        if(!capture_pad){int code=scancode(message.wParam,message.lParam);if(code)key_binding(code);else hint(L"That key is reserved or unsupported. Choose another key, or Escape to cancel.");return true;}
    }
    return IsDialogMessageW(panel,&message)!=0;
}
void rr_controls_write(const std::filesystem::path& path){
    for(int p=0;p<2;++p){const auto& a=g_input_map.p[p];auto section=L"input.p"+std::to_wstring(p+1);
        auto number=[&](const wchar_t* key,int value){auto s=std::to_wstring(value);WritePrivateProfileStringW(section.c_str(),key,s.c_str(),path.c_str());};
        number(L"device",a.device);number(L"pad_type",a.pad_type);number(L"deadzone",a.deadzone_pct);number(L"controller",a.controller_slot);number(L"analog_dpad",a.analog_dpad);
        for(int b=0;b<GB_COUNT;++b){auto key=std::wstring(L"key.")+names[b];number(key.c_str(),a.key[b]);auto& v=a.pad[b];auto value=v.kind==1?L"button:"+std::to_wstring(v.code):v.kind==2?L"axis:"+std::to_wstring(v.code)+(v.axis_dir<0?L":-":L":+"):L"none";key=std::wstring(L"pad.")+names[b];WritePrivateProfileStringW(section.c_str(),key.c_str(),value.c_str(),path.c_str());}
    }
}
unsigned rr_controls_poll(){
    SDL_Event event;while(SDL_PollEvent(&event)){
        gamepad_handle_event(&event);
        if(capture<0||!capture_pad||!armed)continue;
        int id=gamepad_controller_instance(g_input_map.p[player].controller_slot);
        if(event.type==SDL_CONTROLLERBUTTONDOWN&&event.cbutton.which==id)pad_binding({GP_BIND_BUTTON,event.cbutton.button,0});
        if(event.type==SDL_CONTROLLERAXISMOTION&&event.caxis.which==id&&std::abs(event.caxis.value)>20000){
            if(player==0&&event.caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
                hint(L"R2 / RT loads the campaign checkpoint. Choose another input.");
            else pad_binding({GP_BIND_AXIS,event.caxis.axis,event.caxis.value<0?-1:1});
        }
    }
    if(panel&&capture>=0&&capture_pad&&!armed){auto c=SDL_GameControllerFromInstanceID(gamepad_controller_instance(g_input_map.p[player].controller_slot));if(c){armed=true;for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;++b)armed&=!SDL_GameControllerGetButton(c,(SDL_GameControllerButton)b);for(int a=0;a<SDL_CONTROLLER_AXIS_MAX;++a)armed&=std::abs(SDL_GameControllerGetAxis(c,(SDL_GameControllerAxis)a))<12000;}}
    auto c=SDL_GameControllerFromInstanceID(gamepad_controller_instance(0));unsigned now=0;
    if(c){if(SDL_GameControllerGetButton(c,SDL_CONTROLLER_BUTTON_DPAD_LEFT))now|=1;if(SDL_GameControllerGetButton(c,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))now|=2;if(SDL_GameControllerGetButton(c,SDL_CONTROLLER_BUTTON_A))now|=4;if(SDL_GameControllerGetButton(c,SDL_CONTROLLER_BUTTON_Y))now|=8;}
    static unsigned old=0;unsigned pressed=now&~old;old=now;return panel?0:pressed;
}
void rr_controls_shutdown(){gamepad_shutdown();SDL_Quit();DeleteObject(background);}
HWND rr_controls_window(){return panel;}
bool rr_controls_self_test(HWND parent){
    // Exercises the real draft/capture/save paths, using an isolated QA profile.
    InputMap original=g_input_map;bool ok=true;rr_controls_show(parent);
    SendMessageW(panel,WM_COMMAND,104,0);MSG key{};key.hwnd=panel;key.message=WM_KEYDOWN;key.wParam='Q';rr_controls_message(key);
    ok&=g_input_map.p[0].key[GB_A]==SDL_SCANCODE_Q;
    SendMessageW(panel,WM_COMMAND,IDCANCEL,0);ok&=g_input_map.p[0].key[GB_A]==original.p[0].key[GB_A];
    rr_controls_show(parent);SendMessageW(panel,WM_COMMAND,104,0);rr_controls_message(key);
    SendMessageW(panel,WM_COMMAND,IDOK,0);ok&=panel==nullptr&&std::filesystem::exists(config);
    input_map_init_defaults();app_config_load(config.u8string().c_str());ok&=g_input_map.p[0].key[GB_A]==SDL_SCANCODE_Q;
    rr_controls_show(parent);SendMessageW(panel,WM_COMMAND,104,0);key.wParam=VK_F6;rr_controls_message(key);ok&=capture==GB_A;key.wParam=VK_DELETE;rr_controls_message(key);ok&=g_input_map.p[0].key[GB_A]==0;
    SendMessageW(panel,WM_COMMAND,IDCANCEL,0);ok&=g_input_map.p[0].key[GB_A]==SDL_SCANCODE_Q;
    g_input_map=original;return ok;
}
