#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <gdiplus.h>
#include <xinput.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include "ra_service.h"
#include "controls.h"
#include "install.h"

using namespace Gdiplus;
namespace fs = std::filesystem;
static HWND window;
static HINSTANCE instance;
static fs::path root, assets, settings;
static int selected = 0, hover = -1;
static float scale = 1.f;
static float dpi_scale=1.f, origin_x=0.f, origin_y=0.f;
static HFONT body_font;
static HBRUSH dark_brush;
static HANDLE game_process;
static bool qa_launch;
static ULONG_PTR gdiplus_token;
static std::unique_ptr<PrivateFontCollection> display_fonts;
static fs::path qa_capture;
static std::wstring qa_page=L"main";
static int qa_width=0,qa_height=0;
static fs::path qa_ui_result;
static int qa_ui_stage=0,qa_ui_initial=0;
static bool qa_ui_ok=true;
static int qa_ui_launched=-1;
static std::unique_ptr<FontFamily> display_family;
static std::unique_ptr<FontFamily> body_family;
static std::unique_ptr<Image> brand_art;
static std::array<std::unique_ptr<Image>, 3> covers;
static RRRa *ra;
static const wchar_t *titles[] = {L"ROAD RASH", L"ROAD RASH II", L"ROAD RASH 3"};
static const wchar_t *years[] = {L"1991", L"1992", L"1995"};
static const wchar_t *descriptions[] = {
    L"The original California ride. Five roads, fast bikes,\nand rivals who never play fair.",
    L"Take the rivalry across America. Race solo or challenge\na friend in the original two-player split screen.",
    L"Tour de Force. Riders, bikes and roads around the world\nin the trilogy's final Mega Drive ride."
};
static const uint32_t rom_crc[] = {0xDEA53D19u, 0x0876E992u, 0x15785956u};
static const int ra_game[] = {82, 119, 166};
static const Color ink(255, 244, 240, 226), muted(255, 166, 169, 163), yellow(255, 255, 201, 37);
static constexpr int GAME0=100, PLAY=200, VIDEO=201, AUDIO=202, ACHIEVEMENTS=203, NETWORK=204, IMPORT=205, HELP=206;
static std::array<HWND, 3> game_buttons;
static std::vector<HWND> buttons;
static int output_size = 1, filter = 2, volume = 85, music_mode=1;
static int music_volume=20,effects_volume=100;
static bool fullscreen = false, wide_view=true, geometry=true;
static const int widths[] = {960, 1280, 1920, 2560, 3840};
static const int heights[] = {540, 720, 1080, 1440, 2160};
static const char *screen_modes[] = {"raw", "linear", "raw", "raw", "raw", "crt", "trinitron", "composite"};
static const wchar_t *shader_presets[]={L"Presets-ng/Monitor-Balanced.slangp",L"Presets-ng/Monitor-Ambilight-immersive.slangp",L"Presets-ng/Base.slangp"};

static std::wstring wide(const std::string& input) {
    int n = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    std::wstring result(n, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), n);
    if(n)result.pop_back();
    return result;
}
static std::string utf8(const std::wstring& input) {
    int n = WideCharToMultiByte(CP_UTF8, 0, input.data(), (int)input.size(), nullptr, 0, nullptr, nullptr);
    std::string result(n, '\0');
    if (n) WideCharToMultiByte(CP_UTF8, 0, input.data(), (int)input.size(), result.data(), n, nullptr, nullptr);
    return result;
}
static fs::path game_dir(int game) {
    wchar_t module[MAX_PATH]{}; GetModuleFileNameW(nullptr,module,MAX_PATH);
    fs::path sibling=fs::path(module).parent_path()/(L"rr"+std::to_wstring(game+1));
    if(fs::exists(sibling/(L"rr"+std::to_wstring(game+1)+L".exe")))return sibling;
    fs::path packaged = root / L"games" / (L"rr" + std::to_wstring(game+1));
    if (fs::exists(packaged / (L"rr" + std::to_wstring(game+1) + L".exe"))) return packaged;
    return root / L"build/native/bin" / (L"rr" + std::to_wstring(game+1));
}
static fs::path rom_path(int game) {
    fs::path packaged = root / L"roms" / (L"rr" + std::to_wstring(game+1) + L".bin");
    if (fs::exists(packaged)) return packaged;
    return root / L".local/build-inputs" / (L"rr" + std::to_wstring(game+1)) / L"game.bin";
}
static fs::path executable(int game) { return game_dir(game) / (L"rr" + std::to_wstring(game+1) + L".exe"); }
static bool ready(int game) { return fs::exists(executable(game)) && fs::exists(rom_path(game)); }
static void text(Graphics& graphics, const wchar_t *value, float x, float y, float w, float h,
                 float size, Color color = ink, bool display = false) {
    Font font(display ? display_family.get() : FontFamily::GenericSansSerif(), size, display ? FontStyleBold : FontStyleRegular, UnitPixel);
    Font body(body_family?body_family.get():FontFamily::GenericSansSerif(), size, FontStyleRegular, UnitPixel);
    SolidBrush brush(color);
    StringFormat format;
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    Font fallback(L"Segoe UI", size, display?FontStyleBold:FontStyleRegular, UnitPixel);
    auto chosen=display?&font:&body;
    graphics.DrawString(value, -1, chosen->GetLastStatus()==Ok?chosen:&fallback, RectF(x, y, w, h), &format, &brush);
}
// Position the visible lettering, rather than the font's line box/padding.
// This keeps italic overhangs optically centered at every canvas scale.
static RectF lettering(Graphics& graphics, const wchar_t *value, RectF box, float size,
                       Color color, bool italic=false, bool centered=false) {
    FontFamily fallback(L"Segoe UI");
    int style=FontStyleBold|(italic?FontStyleItalic:0);
    auto family=display_family&&display_family->IsStyleAvailable(style)?display_family.get():&fallback;
    GraphicsPath path;
    path.AddString(value,-1,family,style,size,PointF(0,0),StringFormat::GenericTypographic());
    RectF bounds;path.GetBounds(&bounds);
    float x=box.X+(centered?(box.Width-bounds.Width)*.5f:0);
    float y=box.Y+(centered?(box.Height-bounds.Height)*.5f:0);
    Matrix placement;placement.Translate(x-bounds.X,y-bounds.Y);path.Transform(&placement);
    SolidBrush brush(color);graphics.FillPath(&brush,&path);
    return RectF(x,y,bounds.Width,bounds.Height);
}
static int ini_int(const wchar_t *key, int fallback) { return GetPrivateProfileIntW(L"launcher", key, fallback, settings.c_str()); }
static void save_settings() {
    if(!qa_capture.empty()||!qa_ui_result.empty())return;
    auto set = [](const wchar_t *key, int value) { auto v=std::to_wstring(value); WritePrivateProfileStringW(L"launcher",key,v.c_str(),settings.c_str()); };
    set(L"game", selected); set(L"output_size",output_size); set(L"filter",filter); set(L"volume",volume); set(L"fullscreen",fullscreen);
    set(L"wide_view",wide_view);set(L"geometry",geometry);set(L"music",music_mode);
    set(L"music_volume",music_volume);set(L"effects_volume",effects_volume);
}
static void select_game(int game) {
    selected = std::clamp(game, 0, 2);
    for (auto button : game_buttons) InvalidateRect(button, nullptr, FALSE);
    SetWindowTextW(GetDlgItem(window,PLAY),ready(selected)?L"PLAY":L"INSTALL GAME");
    InvalidateRect(window, nullptr, FALSE); save_settings();
}
static void launch() {
    if(!qa_ui_result.empty()){qa_ui_launched=selected;return;}
    if (game_process) return;
    if(!ready(selected)){rr_install_show(window);return;}
    if (!ready(selected)) { MessageBoxW(window,L"This game is not installed yet. Choose Install games and supply your own compatible ROM folder.",L"Install game",MB_OK|MB_ICONINFORMATION); return; }
    fs::path config = game_dir(selected) / L"settings.ini";
    auto set = [&](const wchar_t *section, const wchar_t *key, int value) {
        auto number = std::to_wstring(value); WritePrivateProfileStringW(section,key,number.c_str(),config.c_str());
    };
    set(L"video",L"fullscreen",fullscreen); set(L"video",L"window_scale",3);
    set(L"video",L"linear_filter",filter==1); set(L"audio",L"volume",volume);
    set(L"rewind",L"enabled",0); // Campaign checkpoints use their own persistent slot.
    rr_controls_write(config);
    SetEnvironmentVariableA("RR_CONTROLS_PROFILE","1");
    std::wstring width=std::to_wstring(widths[output_size]), height=std::to_wstring(heights[output_size]);
    SetEnvironmentVariableW(L"RR_OUTPUT_WIDTH",width.c_str()); SetEnvironmentVariableW(L"RR_OUTPUT_HEIGHT",height.c_str());
    SetEnvironmentVariableA("GENESIS_SCREEN",screen_modes[filter]);
    SetEnvironmentVariableW(L"RR_ROOT",root.c_str());
    SetEnvironmentVariableA("RR_WIDE",wide_view?"1":"0");
    SetEnvironmentVariableA("RR_SMOOTH","0");
    SetEnvironmentVariableA("RR_GEOMETRY",geometry?"1":"0");
    SetEnvironmentVariableA("RR_MUSIC",music_mode?"dean-harris":"original");
    auto mv=std::to_wstring(music_volume),ev=std::to_wstring(effects_volume);
    SetEnvironmentVariableW(L"RR_MUSIC_VOLUME",mv.c_str());SetEnvironmentVariableW(L"RR_EFFECTS_VOLUME",ev.c_str());
    fs::path preset;
    if(filter>=2&&filter<=4)preset=root/L"assets/shaders/koko-aio"/shader_presets[filter-2];
    SetEnvironmentVariableW(L"RR_SHADER_PATH",preset.empty()?nullptr:preset.c_str());
    SetEnvironmentVariableW(L"RR_RA_AUTH_FILE",nullptr);
    std::wstring command = L"\"" + executable(selected).wstring() + L"\" \"" + rom_path(selected).wstring() + L"\" --no-launcher --pacing accurate";
    if(qa_launch)command+=L" --turbo --max-frames 2450 --input-script \""+std::wstring(_wgetenv(L"RR_QA_INPUT"))+L"\"";
    STARTUPINFOW start{}; start.cb=sizeof start;
    // Keep a per-launch record so shader/backend failures and old executables
    // can be distinguished from a settings problem in fullscreen reports.
    fs::create_directories(root/L".local/logs");SYSTEMTIME stamp{};GetLocalTime(&stamp);
    wchar_t name[100];swprintf_s(name,L"rr%d-%04u%02u%02u-%02u%02u%02u-%03u.log",selected+1,stamp.wYear,stamp.wMonth,stamp.wDay,stamp.wHour,stamp.wMinute,stamp.wSecond,stamp.wMilliseconds);
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    HANDLE log=CreateFileW((root/L".local/logs"/name).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE input=INVALID_HANDLE_VALUE;
    if(log!=INVALID_HANDLE_VALUE) {
        SetHandleInformation(log,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
        input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        std::string header="Road Rash Trilogy " RR_VERSION " | exe="+utf8(executable(selected).wstring())+" | wide="+std::to_string(wide_view)+" | geometry="+std::to_string(geometry)+" | filter="+std::to_string(filter)+" | music="+std::to_string(music_volume)+" | effects="+std::to_string(effects_volume)+"\n";
        DWORD written;WriteFile(log,header.data(),(DWORD)header.size(),&written,nullptr);
        if(input!=INVALID_HANDLE_VALUE){start.dwFlags|=STARTF_USESTDHANDLES;start.hStdInput=input;start.hStdOutput=start.hStdError=log;}
    }
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable(selected).c_str(),command.data(),nullptr,nullptr,(start.dwFlags&STARTF_USESTDHANDLES)!=0,CREATE_NO_WINDOW,nullptr,game_dir(selected).c_str(),&start,&process)) {
        MessageBoxW(window,L"Could not start the game. Check the installation files and folder permissions.",L"Launch failed",MB_OK|MB_ICONERROR);
    } else {
        game_process=process.hProcess; CloseHandle(process.hThread); ShowWindow(window,SW_MINIMIZE);
    }
    if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);
    SetEnvironmentVariableW(L"RR_RA_AUTH_FILE",nullptr);
    SetEnvironmentVariableW(L"RR_OUTPUT_WIDTH",nullptr); SetEnvironmentVariableW(L"RR_OUTPUT_HEIGHT",nullptr);
    SetEnvironmentVariableA("GENESIS_SCREEN",nullptr);
    for(auto key:{L"RR_ROOT",L"RR_WIDE",L"RR_SMOOTH",L"RR_GEOMETRY",L"RR_MUSIC",L"RR_SHADER_PATH",L"RR_MUSIC_VOLUME",L"RR_EFFECTS_VOLUME"})SetEnvironmentVariableW(key,nullptr);
}

static void import_rom() {
    wchar_t path[32768]{};
    OPENFILENAMEW picker{}; picker.lStructSize=sizeof picker; picker.hwndOwner=window;
    picker.lpstrFilter=L"ROM Mega Drive (*.bin;*.md;*.gen)\0*.bin;*.md;*.gen\0All files\0*.*\0";
    picker.lpstrFile=path; picker.nMaxFile=32768; picker.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&picker)) return;
    std::error_code size_error;
    auto size=fs::file_size(path,size_error);
    if(size_error || size<512 || size>4*1024*1024) { MessageBoxW(window,L"Use a cartridge image no larger than 4 MB.",L"Unsupported ROM",MB_OK);return; }
    std::ifstream file(path,std::ios::binary); std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
    uint32_t crc=0xffffffffu;
    if (data.size()>4*1024*1024 || data.size()<512) { MessageBoxW(window,L"Use a linear binary dump without a copier header.",L"Unsupported ROM",MB_OK); return; }
    for (auto byte:data) { crc^=byte; for(int i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
    crc^=0xffffffffu;
    if (crc!=rom_crc[selected]) { MessageBoxW(window,L"This revision is unsupported. Use the USA/Europe releases listed in the installation guide; Road Rash II must be RR206.",L"Unsupported ROM",MB_OK|MB_ICONINFORMATION); return; }
    auto target=root/L"roms"/(L"rr"+std::to_wstring(selected+1)+L".bin"); fs::create_directories(target.parent_path());
    std::ofstream output(target,std::ios::binary|std::ios::trunc); output.write((const char*)data.data(),data.size());
    if(!output) { MessageBoxW(window,L"Could not save the private ROM copy. Check free space and folder permissions.",L"Import failed",MB_OK|MB_ICONERROR);return; }
    InvalidateRect(window,nullptr,FALSE);
}

static HWND dialog, combo_size, combo_filter, check_full, check_wide, check_geometry, slider_volume, combo_music, username_edit, password_edit;
static HWND slider_music,slider_effects;
static HWND label_volume,label_music,label_effects;
static void audio_labels(){
    auto label=[](HWND target,const wchar_t *prefix,int value){auto s=std::wstring(prefix)+std::to_wstring(value)+L"%";SetWindowTextW(target,s.c_str());};
    label(label_volume,L"Master volume — ",(int)SendMessageW(slider_volume,TBM_GETPOS,0,0));
    label(label_music,L"Remake music — ",(int)SendMessageW(slider_music,TBM_GETPOS,0,0));
    label(label_effects,L"Remake effects — ",(int)SendMessageW(slider_effects,TBM_GETPOS,0,0));
}
static int dialog_page;
static float dialog_scale=1;
static HFONT dialog_font;
static void video_dialog();
static LRESULT CALLBACK dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch(message) {
    case WM_CREATE: {
        dialog_font=CreateFontW(-(int)(16*dialog_scale),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Barlow");
        auto control=[&](const wchar_t* type,const wchar_t* caption,DWORD style,int x,int y,int w,int h,int id) {
            HWND c=CreateWindowExW(0,type,caption,WS_CHILD|WS_VISIBLE|style,(int)(x*dialog_scale),(int)(y*dialog_scale),(int)(w*dialog_scale),(int)(h*dialog_scale),hwnd,(HMENU)(INT_PTR)id,instance,nullptr);
            SendMessageW(c,WM_SETFONT,(WPARAM)dialog_font,TRUE);if((style&BS_TYPEMASK)==BS_AUTOCHECKBOX&&!wcscmp(type,L"BUTTON"))SetWindowTheme(c,L"",L"");return c;
        };
        if(dialog_page==0) {
            control(L"STATIC",L"Output resolution",0,28,30,420,25,0);
            combo_size=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,28,60,420,220,1);
            for(int i=0;i<5;++i) { auto label=std::to_wstring(widths[i])+L" × "+std::to_wstring(heights[i]); SendMessageW(combo_size,CB_ADDSTRING,0,(LPARAM)label.c_str()); }
            SendMessageW(combo_size,CB_SETCURSEL,output_size,0);
            control(L"STATIC",L"Display filter",0,28,112,420,25,0);
            combo_filter=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,28,142,420,220,2);
            for(auto label:{L"Original pixels",L"Linear",L"koko-aio · Balanced",L"koko-aio · Ambilight",L"koko-aio · Classic CRT",L"Light CRT",L"Trinitron",L"Composite"}) SendMessageW(combo_filter,CB_ADDSTRING,0,(LPARAM)label);
            SendMessageW(combo_filter,CB_SETCURSEL,filter,0);
            check_full=control(L"BUTTON",L"Fullscreen",BS_AUTOCHECKBOX|WS_TABSTOP,28,192,420,28,3);
            SendMessageW(check_full,BM_SETCHECK,fullscreen?BST_CHECKED:BST_UNCHECKED,0);
            check_wide=control(L"BUTTON",L"Expand the sides to 16:9 (experimental)",BS_AUTOCHECKBOX|WS_TABSTOP,28,226,440,28,4);
            SendMessageW(check_wide,BM_SETCHECK,wide_view?BST_CHECKED:BST_UNCHECKED,0);
            check_geometry=control(L"BUTTON",L"Continuous zoom and smooth movement",BS_AUTOCHECKBOX|WS_TABSTOP,28,260,440,28,5);
            SendMessageW(check_geometry,BM_SETCHECK,geometry?BST_CHECKED:BST_UNCHECKED,0);
            control(L"STATIC",L"Pixel-art proportions are preserved with every filter.\nWidescreen CRT uses the complete viewport.\nSmooth zoom supports solo and split-screen races.\nMenus and paused motion keep their original timing.",0,28,310,440,90,0);
        } else if(dialog_page==1) {
            label_volume=control(L"STATIC",L"Master volume",0,28,22,420,24,0);
            slider_volume=control(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,28,46,420,32,10);
            SendMessageW(slider_volume,TBM_SETRANGE,TRUE,MAKELONG(0,100)); SendMessageW(slider_volume,TBM_SETPOS,TRUE,volume);
            label_music=control(L"STATIC",L"Remake music",0,28,88,420,24,0);
            slider_music=control(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,28,112,420,32,11);
            SendMessageW(slider_music,TBM_SETRANGE,TRUE,MAKELONG(0,100));SendMessageW(slider_music,TBM_SETPOS,TRUE,music_volume);
            label_effects=control(L"STATIC",L"Effects with remake soundtrack",0,28,154,440,24,0);
            slider_effects=control(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS|WS_TABSTOP,28,178,420,32,12);
            SendMessageW(slider_effects,TBM_SETRANGE,TRUE,MAKELONG(0,200));SendMessageW(slider_effects,TBM_SETPOS,TRUE,effects_volume);
            audio_labels();
            control(L"STATIC",L"Audio & music",0,28,222,420,26,0);
            combo_music=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,28,250,420,150,20);
            for(auto label:{L"Original Mega Drive soundtrack",L"Dean Harris remakes"})SendMessageW(combo_music,CB_ADDSTRING,0,(LPARAM)label);
            SendMessageW(combo_music,CB_SETCURSEL,music_mode,0);
            control(L"STATIC",L"Remake music starts at 20% to keep effects clear.\nPausing preserves the music position.\nMissing remake tracks fall back to the original audio.\nInstaller packages include the remake music pack.",0,28,300,440,96,0);
        } else if(dialog_page==2) {
            control(L"STATIC",L"Local in-game achievements",0,28,28,440,32,0);
            control(L"STATIC",L"Press F6 in game to open your achievement panel.\nUnlocks appear on screen and are saved on this PC.\nNo account or service sign-in is needed.",0,28,86,440,84,0);
            control(L"STATIC",L"Each game keeps its own local progress:\n\nFirst Start / Engine Warm / Race Pace\nOne Minute on the Road / Five Minutes on the Road\nBack on the Asphalt",0,28,200,440,124,0);
            control(L"STATIC",L"Local progress is separate from official\nRetroAchievements service unlocks.",0,28,350,440,52,0);
        } else {
            control(L"STATIC",L"RetroAchievements",0,28,24,420,30,0);
            control(L"STATIC",L"Sign in to load official achievements.\nWindows protects your token on this computer.",0,28,66,440,48,0);
            control(L"STATIC",L"Username",0,28,128,420,24,0);
            username_edit=control(L"EDIT",L"",ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,28,155,420,32,10);
            control(L"STATIC",L"Password",0,28,204,420,24,0);
            password_edit=control(L"EDIT",L"",ES_AUTOHSCROLL|ES_PASSWORD|WS_BORDER|WS_TABSTOP,28,232,420,32,11);
            SendMessageW(username_edit,EM_SETLIMITTEXT,128,0);SendMessageW(password_edit,EM_SETLIMITTEXT,128,0);
            control(L"STATIC",L"Casual mode. Hardcore is disabled during port validation.\nYour password is not saved.",0,28,282,440,48,0);
            control(L"BUTTON",L"Sign in",BS_PUSHBUTTON|WS_TABSTOP,28,344,132,36,50);
            control(L"BUTTON",L"Sign out",BS_PUSHBUTTON|WS_TABSTOP,174,344,145,36,51);
            control(L"BUTTON",L"View achievements",BS_PUSHBUTTON|WS_TABSTOP,332,344,132,36,52);
            SetTimer(hwnd,1,200,nullptr);
        }
        control(L"BUTTON",dialog_page==2?L"Close":L"Save",BS_DEFPUSHBUTTON|WS_TABSTOP,332,416,132,38,IDOK);
        return 0;
    }
    case WM_TIMER: rr_ra_update(ra,0); InvalidateRect(hwnd,nullptr,FALSE); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);
        if(dialog_page==3) { SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,207,66));SelectObject(dc,body_font);
            auto status=wide(rr_ra_status(ra));RECT r{28,386,465,414};DrawTextW(dc,status.c_str(),-1,&r,DT_LEFT|DT_END_ELLIPSIS|DT_SINGLELINE); }
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORBTN: SetTextColor((HDC)wparam,RGB(244,240,226));SetBkColor((HDC)wparam,RGB(26,29,31));return (LRESULT)dark_brush;
    case WM_HSCROLL:if(dialog_page==1)audio_labels();return 0;
    case WM_COMMAND:
        if(LOWORD(wparam)==50) {
            wchar_t user[129]{},pass[129]{};GetWindowTextW(username_edit,user,129);GetWindowTextW(password_edit,pass,129);
            std::string u=utf8(user),p=utf8(pass);rr_ra_login(ra,u.c_str(),p.c_str());
            SecureZeroMemory(pass,sizeof pass);if(!p.empty())SecureZeroMemory(p.data(),p.size());SetWindowTextW(password_edit,L"");return 0;
        }
        if(LOWORD(wparam)==51){rr_ra_logout(ra);return 0;}
        if(LOWORD(wparam)==52){auto url=L"https://retroachievements.org/game/"+std::to_wstring(ra_game[selected]);ShellExecuteW(hwnd,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;}
        if(LOWORD(wparam)==IDOK && HIWORD(wparam)==BN_CLICKED) {
            if(dialog_page==0){output_size=(int)SendMessageW(combo_size,CB_GETCURSEL,0,0);filter=(int)SendMessageW(combo_filter,CB_GETCURSEL,0,0);fullscreen=SendMessageW(check_full,BM_GETCHECK,0,0)==BST_CHECKED;wide_view=SendMessageW(check_wide,BM_GETCHECK,0,0)==BST_CHECKED;geometry=SendMessageW(check_geometry,BM_GETCHECK,0,0)==BST_CHECKED;}
            if(dialog_page==1){volume=(int)SendMessageW(slider_volume,TBM_GETPOS,0,0);music_mode=(int)SendMessageW(combo_music,CB_GETCURSEL,0,0);music_volume=(int)SendMessageW(slider_music,TBM_GETPOS,0,0);effects_volume=(int)SendMessageW(slider_effects,TBM_GETPOS,0,0);}
            save_settings();DestroyWindow(hwnd);return 0;
        }break;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY: dialog=nullptr;DeleteObject(dialog_font);dialog_font=nullptr;EnableWindow(window,TRUE);if(qa_capture.empty()&&qa_ui_result.empty())SetForegroundWindow(window);InvalidateRect(window,nullptr,FALSE);return 0;
    }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}
static void show_dialog(int page) {
    if(dialog){SetForegroundWindow(dialog);return;}
    dialog_page=page;
    RECT r,work;GetWindowRect(window,&r);SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);EnableWindow(window,FALSE);
    dialog_scale=std::min(dpi_scale,(work.bottom-work.top)/510.f);int dw=(int)(510*dialog_scale),dh=(int)(510*dialog_scale);
    dialog=CreateWindowExW(WS_EX_DLGMODALFRAME|(qa_capture.empty()?0:WS_EX_NOACTIVATE),L"RRSettings",page==0?L"Video & CRT":page==1?L"Audio & music":L"Achievements",
        WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,r.left+(r.right-r.left-dw)/2,std::max(work.top,r.top+(r.bottom-r.top-dh)/2),dw,dh,window,nullptr,instance,nullptr);
}

static void draw_button(DRAWITEMSTRUCT *item) {
    Graphics graphics(item->hDC);graphics.SetSmoothingMode(SmoothingModeAntiAlias);graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    float w=(float)(item->rcItem.right-item->rcItem.left),h=(float)(item->rcItem.bottom-item->rcItem.top);
    graphics.ScaleTransform(scale,scale);w/=scale;h/=scale;
    int id=(int)item->CtlID;bool hot=hover==id,focus=(item->itemState&ODS_FOCUS)!=0,pressed=(item->itemState&ODS_SELECTED)!=0;
    if(id>=GAME0&&id<GAME0+3){int game=id-GAME0;bool active=selected==game;
        SolidBrush background(active?Color(255,38,42,43):Color(255,23,26,28));graphics.FillRectangle(&background,0.f,0.f,w,h);
        if(covers[game]&&covers[game]->GetLastStatus()==Ok){float factor=std::min((w-20)/covers[game]->GetWidth(),(h-77)/covers[game]->GetHeight());float cw=covers[game]->GetWidth()*factor,ch=covers[game]->GetHeight()*factor;graphics.DrawImage(covers[game].get(),RectF((w-cw)*.5f,10,cw,ch));}
        SolidBrush selection(active?yellow:Color(255,72,77,76));graphics.FillRectangle(&selection,10.f,h-61,w-20,active?3.f:1.f);
        text(graphics,titles[game],12,h-48,w-24,33,25,active?yellow:ink,true);
        auto state=std::wstring(years[game])+(ready(game)?L" / Ready":L" / Not installed");text(graphics,state.c_str(),12,h-18,w-24,18,13,muted);
        if(active||focus||hot){Pen border(active?yellow:Color(255,168,171,165),focus?2.f:1.f);graphics.DrawRectangle(&border,1.f,1.f,w-2,h-2);}
    }else{
        bool primary=id==PLAY;SolidBrush background(primary?(pressed?Color(255,214,157,19):yellow):(hot||focus?Color(255,49,53,53):Color(255,33,37,39)));
        graphics.FillRectangle(&background,0.f,0.f,w,h);
        wchar_t label[128];GetWindowTextW(item->hwndItem,label,128);
        if(primary)lettering(graphics,label,RectF(18,4,w-36,h-8),32,Color(255,17,20,22),true,true);
        else text(graphics,label,18,(h-25)*.5f,w-36,h,18,ink);
        if(focus){Pen pen(primary?Color(255,16,19,21):yellow,2);graphics.DrawRectangle(&pen,4.f,4.f,w-8,h-8);}
    }
}
static LRESULT CALLBACK button_subclass(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR) {
    if(msg==WM_GETDLGCODE){auto* input=(MSG*)lp;return DefSubclassProc(hwnd,msg,wp,lp)|DLGC_WANTARROWS|((input&&input->message==WM_KEYDOWN&&input->wParam==VK_RETURN)?DLGC_WANTMESSAGE:0);}
    if(msg==WM_KEYDOWN) {
        if(wp==VK_LEFT){select_game((selected+2)%3);return 0;}
        if(wp==VK_RIGHT){select_game((selected+1)%3);return 0;}
        if(wp==VK_RETURN){auto id=GetDlgCtrlID(hwnd);if(id>=GAME0&&id<GAME0+3)launch();else SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)hwnd);return 0;}
    }
    if(msg==WM_MOUSEMOVE){int id=GetDlgCtrlID(hwnd);if(hover!=id){hover=id;InvalidateRect(hwnd,nullptr,FALSE);}TRACKMOUSEEVENT event{sizeof event,TME_LEAVE,hwnd,0};TrackMouseEvent(&event);}
    if(msg==WM_MOUSELEAVE){hover=-1;InvalidateRect(hwnd,nullptr,FALSE);}
    return DefSubclassProc(hwnd,msg,wp,lp);
}
static void layout() {
    RECT client;GetClientRect(window,&client);scale=std::min(client.right/1280.f,client.bottom/800.f);
    origin_x=(client.right-1280.f*scale)*.5f;origin_y=(client.bottom-800.f*scale)*.5f;
    auto move=[&](HWND hwnd,int x,int y,int w,int h){MoveWindow(hwnd,(int)(origin_x+x*scale),(int)(origin_y+y*scale),(int)(w*scale),(int)(h*scale),TRUE);};
    for(int i=0;i<3;++i)move(game_buttons[i],42+i*292,240,264,414);
    for(size_t i=3;i<buttons.size();++i){int id=GetDlgCtrlID(buttons[i]);
        if(id==PLAY)move(buttons[i],952,692,284,58);
        else if(id>=VIDEO&&id<=NETWORK)move(buttons[i],952,267+(id-VIDEO)*62,284,48);
        else if(id==IMPORT)move(buttons[i],952,545,284,42);
        else if(id==HELP)move(buttons[i],1070,44,166,40);
    }
    InvalidateRect(window,nullptr,FALSE);
}
static void render(HWND hwnd,HDC dc) {
    RECT r;GetClientRect(hwnd,&r);
    Bitmap buffer(std::max(1L,r.right),std::max(1L,r.bottom),PixelFormat32bppARGB);
    Graphics g(&buffer);g.Clear(Color(255,16,19,21));g.TranslateTransform(origin_x,origin_y);g.ScaleTransform(scale,scale);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    SolidBrush header(Color(255,23,26,28)),rule(Color(255,62,66,65)), accent(yellow);
    g.FillRectangle(&header,0.f,0.f,1280.f,118.f);
    if(brand_art&&brand_art->GetLastStatus()==Ok){g.DrawImage(brand_art.get(),RectF(0,0,1280,175),0.f,250.f,(float)brand_art->GetWidth(),280.f,UnitPixel);}
    auto title=lettering(g,L"ROAD RASH TRILOGY",RectF(44,36,800,70),69,ink);
    lettering(g,L"RECOMP",RectF(title.GetRight()+14,title.GetBottom()-25,200,34),34,yellow,true);
    g.FillRectangle(&rule,42.f,117.f,1194.f,1.f);
    text(g,L"CHOOSE YOUR RIDE",42,148,890,65,49,ink,true);
    text(g,L"Three classics. Your original games. Choose a ride and start your engine.",44,208,870,25,18,muted);
    text(g,L"SETTINGS",952,224,284,36,27,ink,true);
    g.FillRectangle(&rule,924.f,240.f,1.f,510.f);
    text(g,descriptions[selected],44,687,840,58,19,ink);
    std::wstring state=ready(selected)?L"Version " RR_VERSION L" / locally built from your ROM":L"Choose Install games to prepare this title with your own ROM.";
    text(g,state.c_str(),44,761,840,22,14,muted);
    std::wstring ra_state=L"Local achievements / F6 in game\nYour progress stays on this PC.";
    text(g,ra_state.c_str(),952,609,284,48,15,muted);
    text(g,L"Arrow keys / D-pad: choose     Enter / A: play",42,128,1000,20,14,muted);
    text(g,L"v0.60 / Windows x64",1040,762,200,22,14,muted);
    Graphics screen(dc);screen.DrawImage(&buffer,0,0);
}
static void paint(HWND hwnd){PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);render(hwnd,dc);EndPaint(hwnd,&ps);}
static bool capture_window(HWND target){
    RECT r{};GetWindowRect(target,&r);int w=r.right-r.left,h=r.bottom-r.top;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h);
    auto previous=SelectObject(dc,bitmap);BOOL printed=PrintWindow(target,dc,2);
    SelectObject(dc,previous);bool saved=false;
    if(printed){Bitmap output(bitmap,nullptr);CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};fs::create_directories(qa_capture.parent_path());saved=output.Save(qa_capture.c_str(),&png,nullptr)==Ok;}
    DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);return saved;
}
static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    switch(message){
    case WM_CREATE:{window=hwnd;
        auto button=[&](const wchar_t* label,int id){HWND b=CreateWindowExW(0,L"BUTTON",label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,hwnd,(HMENU)(INT_PTR)id,instance,nullptr);SetWindowSubclass(b,button_subclass,1,0);buttons.push_back(b);return b;};
        for(int i=0;i<3;++i)game_buttons[i]=button(titles[i],GAME0+i);
        button(L"PLAY",PLAY);button(L"Video & CRT",VIDEO);button(L"Audio & music",AUDIO);button(L"Achievements",ACHIEVEMENTS);button(L"Controls",NETWORK);button(L"Install games",IMPORT);button(L"About",HELP);
        SetWindowTextW(GetDlgItem(hwnd,PLAY),ready(selected)?L"PLAY":L"INSTALL GAME");SetTimer(hwnd,1,150,nullptr);layout();return 0;}
    case WM_PAINT:paint(hwnd);return 0;
    case WM_PRINTCLIENT:render(hwnd,(HDC)wp);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:layout();return 0;
    case WM_GETMINMAXINFO:{auto* info=(MINMAXINFO*)lp;info->ptMinTrackSize={(LONG)(920*dpi_scale),(LONG)(600*dpi_scale)};return 0;}
    case WM_DRAWITEM:draw_button((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_COMMAND:{int id=LOWORD(wp);if(id>=GAME0&&id<GAME0+3){select_game(id-GAME0);return 0;}
        switch(id){case PLAY:launch();break;case VIDEO:show_dialog(0);break;case AUDIO:show_dialog(1);break;case ACHIEVEMENTS:show_dialog(2);break;case IMPORT:rr_install_show(hwnd);break;
        case NETWORK:rr_controls_show(hwnd);break;
        case HELP:MessageBoxW(hwnd,L"Road Rash Trilogy Recomp - version " RR_VERSION L"\n\nAn independent Windows fan project using your original Mega Drive ROMs. Native translated 68000 code runs with a fallback for remaining paths.\n\nContinuous sprite zoom, widescreen, CRT filters and local achievements. Network play is not implemented.\n\nClassic covers: libretro-thumbnails. Fonts: Barlow / OFL. CRT: koko-aio. See CREDITS.md and THIRD_PARTY_NOTICES.md.",L"About",MB_OK);break;}return 0;}
    case WM_TIMER:{if(!qa_ui_result.empty()){
            if(qa_ui_stage==0){qa_ui_initial=selected;PostMessageW(game_buttons[selected],WM_KEYDOWN,VK_RIGHT,0);}
            if(qa_ui_stage==1){qa_ui_ok&=selected==(qa_ui_initial+1)%3;PostMessageW(game_buttons[qa_ui_initial],WM_KEYDOWN,VK_RETURN,0);}
            if(qa_ui_stage==2){qa_ui_ok&=qa_ui_launched==(qa_ui_initial+1)%3;PostMessageW(game_buttons[qa_ui_initial],WM_KEYDOWN,VK_LEFT,0);}
            if(qa_ui_stage==3){qa_ui_ok&=selected==qa_ui_initial;PostMessageW(GetDlgItem(hwnd,VIDEO),WM_KEYDOWN,VK_RETURN,0);}
            if(qa_ui_stage==4){qa_ui_ok&=dialog!=nullptr;if(dialog)DestroyWindow(dialog);PostMessageW(GetDlgItem(hwnd,NETWORK),WM_KEYDOWN,VK_RETURN,0);}
            if(qa_ui_stage==5){qa_ui_ok&=rr_controls_window()!=nullptr;if(rr_controls_window())SendMessageW(rr_controls_window(),WM_CLOSE,0,0);qa_ui_ok&=rr_controls_self_test(hwnd);fs::create_directories(qa_ui_result.parent_path());std::ofstream output(qa_ui_result);output<<"{\"queued_keyboard_and_mapping\":"<<(qa_ui_ok?"true":"false")<<",\"same_card_arrow_enter\":"<<(qa_ui_launched==(qa_ui_initial+1)%3?"true":"false")<<"}\n";output.close();DestroyWindow(hwnd);return 0;}
            ++qa_ui_stage;return 0;
        }if(!qa_capture.empty()){
            static int ticks=0;if(++ticks<3)return 0;
            HWND target=rr_controls_window();if(!target)target=rr_install_window();if(!target)target=dialog;if(!target)target=hwnd;
            bool ok=capture_window(target);if(target!=hwnd)DestroyWindow(target);DestroyWindow(hwnd);PostQuitMessage(ok?0:4);return 0;
        }rr_ra_update(ra,0);
        if(game_process&&WaitForSingleObject(game_process,0)==WAIT_OBJECT_0){DWORD exit;GetExitCodeProcess(game_process,&exit);CloseHandle(game_process);game_process=nullptr;ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);if(exit)MessageBoxW(hwnd,L"The game exited with an error. Diagnostic logs are available in the .local/logs folder.",L"Game session ended",MB_OK|MB_ICONWARNING);}
        unsigned pressed=rr_controls_poll();if(!dialog&&IsWindowEnabled(hwnd)&&!game_process){if(pressed&1)select_game((selected+2)%3);if(pressed&2)select_game((selected+1)%3);if(pressed&4)launch();if(pressed&8)show_dialog(0);}
        return 0;}
    case WM_KEYDOWN:if(wp==VK_LEFT)select_game((selected+2)%3);if(wp==VK_RIGHT)select_game((selected+1)%3);if(wp==VK_RETURN)launch();return 0;
    case WM_CLOSE:if(rr_install_busy()){rr_install_show(hwnd);return 0;}if(game_process){MessageBoxW(hwnd,L"Close the game window before exiting the launcher.",L"Game running",MB_OK);return 0;}DestroyWindow(hwnd);return 0;
    case WM_DESTROY:if(qa_capture.empty()&&qa_ui_result.empty())save_settings();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}

int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR args,int show) {
    instance=app;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(int i=1;i<argc;++i){if(!wcscmp(argv[i],L"--qa-capture")&&i+1<argc)qa_capture=fs::absolute(argv[++i]);else if(!wcscmp(argv[i],L"--qa-ui-test")&&i+1<argc)qa_ui_result=fs::absolute(argv[++i]);else if(!wcscmp(argv[i],L"--qa-page")&&i+1<argc)qa_page=argv[++i];else if(!wcscmp(argv[i],L"--qa-size")&&i+2<argc){qa_width=_wtoi(argv[++i]);qa_height=_wtoi(argv[++i]);}}
    LocalFree(argv);bool qa=!qa_capture.empty()||!qa_ui_result.empty();if(qa)SetEnvironmentVariableW(L"RR_QA_CAPTURE",L"1");
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    dpi_scale=GetDpiForSystem()/96.f;
    wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);root=fs::path(path).parent_path();
    for(int i=0;i<6&&!fs::exists(root/L"assets/launcher/rr1.png");++i){if(root==root.parent_path())break;root=root.parent_path();}
    assets=root/L"assets/launcher";
    if(!fs::exists(assets/L"rr1.png")) { MessageBoxW(nullptr,L"Launcher artwork is missing. Extract the complete release package before opening the app.",L"Missing files",MB_OK|MB_ICONERROR);return 1; }
    fs::create_directories(root/L".local");settings=root/L".local/launcher.ini";
    selected=std::clamp(ini_int(L"game",0),0,2);output_size=std::clamp(ini_int(L"output_size",1),0,4);filter=std::clamp(ini_int(L"filter",2),0,7);volume=std::clamp(ini_int(L"volume",85),0,100);fullscreen=ini_int(L"fullscreen",0)!=0;
    wide_view=ini_int(L"wide_view",1)!=0;geometry=ini_int(L"geometry",1)!=0;
    bool has_remakes=fs::exists(root/L".local/music/dean-harris/rr1.tsv")||fs::exists(root/L"music/dean-harris/rr1.tsv");
    music_mode=std::clamp(ini_int(L"music",has_remakes?1:0),0,1);
    music_volume=std::clamp(ini_int(L"music_volume",20),0,100);effects_volume=std::clamp(ini_int(L"effects_volume",100),0,200);
    GdiplusStartupInput input;GdiplusStartup(&gdiplus_token,&input,nullptr);
    AddFontResourceExW((assets/L"BarlowCondensed-Bold.ttf").c_str(),FR_PRIVATE,nullptr);
    display_fonts=std::make_unique<PrivateFontCollection>();
    display_fonts->AddFontFile((assets/L"BarlowCondensed-Bold.ttf").c_str());
    display_fonts->AddFontFile((assets/L"BarlowCondensed-BoldItalic.ttf").c_str());
    AddFontResourceExW((assets/L"Barlow-Regular.ttf").c_str(),FR_PRIVATE,nullptr);
    display_fonts->AddFontFile((assets/L"Barlow-Regular.ttf").c_str());
    display_family=std::make_unique<FontFamily>(L"Barlow Condensed",display_fonts.get());
    body_family=std::make_unique<FontFamily>(L"Barlow",display_fonts.get());
    brand_art=std::make_unique<Image>((assets/L"brand-050.png").c_str());
    for(int i=0;i<3;++i)covers[i]=std::make_unique<Image>((assets/(L"rr"+std::to_wstring(i+1)+L".png")).c_str());
    body_font=CreateFontW(-(int)(18*dpi_scale),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Barlow");
    dark_brush=CreateSolidBrush(RGB(26,29,31));InitCommonControls();
    rr_controls_init(app,body_font,qa_ui_result.empty()?root/L".local/controls.ini":qa_ui_result.parent_path()/L"qa-controls.ini");rr_install_init(app,body_font,root);
    if(args && !wcscmp(args,L"--qa-launch-crt")){
        const wchar_t *replay=_wgetenv(L"RR_QA_INPUT");if(!replay||!fs::exists(replay))return 3;
        qa_launch=true;selected=0;filter=2;wide_view=true;fullscreen=false;output_size=1;
        if(const wchar_t *game=_wgetenv(L"RR_QA_GAME"))selected=std::clamp(_wtoi(game)-1,0,2);
        SetEnvironmentVariableW(L"RR_TEST_HIDDEN",L"1");SetEnvironmentVariableW(L"SDL_RENDER_VSYNC",L"0");SetEnvironmentVariableW(L"SDL_AUDIODRIVER",L"dummy");
        launch();if(!game_process)return 2;DWORD code=2;if(WaitForSingleObject(game_process,120000)==WAIT_OBJECT_0)GetExitCodeProcess(game_process,&code);
        CloseHandle(game_process);return (int)code;
    }
    ra=nullptr; // Default experience is local and never requests a service login.
    WNDCLASSW klass{};klass.hInstance=app;klass.lpfnWndProc=proc;klass.lpszClassName=L"RRTrilogyLauncher";klass.hCursor=LoadCursor(nullptr,IDC_ARROW);klass.hIcon=LoadIconW(app,MAKEINTRESOURCEW(101));RegisterClassW(&klass);
    WNDCLASSW dialog_class=klass;dialog_class.lpfnWndProc=dialog_proc;dialog_class.lpszClassName=L"RRSettings";dialog_class.hbrBackground=dark_brush;RegisterClassW(&dialog_class);
    RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int initial_w=std::min((LONG)(1300*dpi_scale),work.right-work.left),initial_h=std::min((LONG)(850*dpi_scale),work.bottom-work.top);
    if(qa_width>=920&&qa_height>=600){initial_w=qa_width;initial_h=qa_height;}
    HWND hwnd=CreateWindowExW(qa?WS_EX_NOACTIVATE:0,klass.lpszClassName,L"Road Rash Trilogy Recomp - 0.60",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,work.left+(work.right-work.left-initial_w)/2,work.top+(work.bottom-work.top-initial_h)/2,initial_w,initial_h,nullptr,nullptr,app,nullptr);
    ShowWindow(hwnd,qa?SW_SHOWNOACTIVATE:show);UpdateWindow(hwnd);
    if(!qa_capture.empty()){if(qa_page==L"controls")rr_controls_show(hwnd);else if(qa_page==L"install")rr_install_show(hwnd);else if(qa_page==L"video")show_dialog(0);else if(qa_page==L"audio")show_dialog(1);else if(qa_page==L"achievements")show_dialog(2);}
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){if(rr_controls_message(message)||rr_install_message(message))continue;if(dialog&&IsDialogMessageW(dialog,&message))continue;if(!dialog&&IsDialogMessageW(hwnd,&message))continue;TranslateMessage(&message);DispatchMessageW(&message);}
    rr_controls_shutdown();rr_ra_destroy(ra);for(auto& image:covers)image.reset();display_family.reset();body_family.reset();brand_art.reset();display_fonts.reset();
    RemoveFontResourceExW((assets/L"BarlowCondensed-Bold.ttf").c_str(),FR_PRIVATE,nullptr);
    RemoveFontResourceExW((assets/L"Barlow-Regular.ttf").c_str(),FR_PRIVATE,nullptr);
    DeleteObject(body_font);DeleteObject(dark_brush);GdiplusShutdown(gdiplus_token);CoUninitialize();return 0;
}
