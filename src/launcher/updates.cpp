#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <string>
#include <algorithm>
#include <chrono>
#include <cwctype>
#include "updates.h"
#ifndef RR_VERSION
#define RR_VERSION "0.60"
#endif
namespace {
namespace fs=std::filesystem;
constexpr UINT LINE=WM_APP+221,DONE=WM_APP+222;
HINSTANCE app;HFONT font,panel_font;HBRUSH brush;HWND parent,panel,status,details,download,check_button,close_button;
fs::path root,python,script,result,offer,job,host_exe;HANDLE worker_job,instance_lock,parent_process;
bool qa,busy,host_mode,applying,cancelled,recovering,can_reopen,waiting;bool (*ready)();float dpi=1;
std::wstring action;unsigned waits;
std::wstring quote(const fs::path& p){return L"\""+p.wstring()+L"\"";}
std::wstring wide(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,0);if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n);return w;}
std::wstring value(const wchar_t *key){wchar_t text[32768]{};GetPrivateProfileStringW(L"update",key,L"",text,32768,result.c_str());return text;}
void append(const std::wstring& line){if(!details)return;int n=GetWindowTextLengthW(details);SendMessageW(details,EM_SETSEL,n,n);auto s=line+L"\r\n";SendMessageW(details,EM_REPLACESEL,FALSE,(LPARAM)s.c_str());}
void set_status(const std::wstring& s){if(status)SetWindowTextW(status,s.c_str());}
void controls(){bool tools=fs::exists(python)&&fs::exists(script);if(check_button)EnableWindow(check_button,!busy&&!host_mode&&tools);if(download)EnableWindow(download,!busy&&!host_mode&&tools&&!offer.empty());if(close_button)EnableWindow(close_button,!applying);}
void start(const std::wstring& mode){
    if(busy)return;result=host_mode?job/L"result.ini":root/L".local/updates/result.ini";
    std::error_code e;fs::create_directories(result.parent_path(),e);if(e){set_status(L"Updates need a writable installation folder. Your games can still be played.");return;}fs::remove(result,e);
    std::wstring command=quote(python)+L" -X utf8 "+quote(script)+L" "+mode+L" --root "+quote(root)+L" --current " RR_VERSION L" --result "+quote(result);
    if(mode==L"prepare")command+=L" --asset "+quote(offer);
    if(mode==L"apply"||mode==L"recover"||mode==L"cleanup")command+=L" --job "+quote(job);
    SECURITY_ATTRIBUTES sa{sizeof sa,nullptr,TRUE};HANDLE read=nullptr,write=nullptr;
    if(!CreatePipe(&read,&write,&sa,0)){set_status(L"Could not create the update log pipe.");return;}SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    STARTUPINFOW si{};si.cb=sizeof si;si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=si.hStdError=write;si.hStdInput=input;
    PROCESS_INFORMATION pi{};worker_job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool ok=worker_job&&SetInformationJobObject(worker_job,JobObjectExtendedLimitInformation,&limits,sizeof limits)&&
        CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,root.c_str(),&si,&pi);
    CloseHandle(write);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
    if(!ok){CloseHandle(read);if(worker_job)CloseHandle(worker_job);worker_job=nullptr;set_status(L"Could not start the update helper. Reinstall the complete package.");return;}
    if(!AssignProcessToJobObject(worker_job,pi.hProcess)){TerminateProcess(pi.hProcess,1);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);CloseHandle(read);CloseHandle(worker_job);worker_job=nullptr;set_status(L"Could not create a cancellable update process.");return;}
    busy=true;action=mode;applying=false;controls();HWND receiver=parent?parent:panel;
    ResumeThread(pi.hThread);CloseHandle(pi.hThread);
    auto log_path=host_mode?job/L"update.log":root/L".local/updates/update.log";
    std::thread([read,process=pi.hProcess,receiver,log_path]{std::string pending;char data[4096];DWORD n;std::ofstream log(log_path,std::ios::binary|std::ios::app);
        while(ReadFile(read,data,sizeof data,&n,nullptr)&&n){log.write(data,n);log.flush();pending.append(data,n);size_t at;while((at=pending.find('\n'))!=std::string::npos){auto *line=new std::wstring(wide(pending.substr(0,at)));pending.erase(0,at+1);if(!PostMessageW(receiver,LINE,0,(LPARAM)line))delete line;}}
        CloseHandle(read);WaitForSingleObject(process,INFINITE);DWORD code=1;GetExitCodeProcess(process,&code);CloseHandle(process);PostMessageW(receiver,DONE,code,0);
    }).detach();
}
bool launch_host(bool recover){
    FILETIME created,exited,kernel,user;GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
    ULARGE_INTEGER ticks;ticks.LowPart=created.dwLowDateTime;ticks.HighPart=created.dwHighDateTime;
    auto args=std::wstring(recover?L"--recover-update ":L"--apply-update ")+quote(root)+L" "+quote(job)+L" "+std::to_wstring(GetCurrentProcessId())+L" "+std::to_wstring(ticks.QuadPart);
    SHELLEXECUTEINFOW info{};info.cbSize=sizeof info;info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpFile=host_exe.c_str();info.lpParameters=args.c_str();info.lpDirectory=host_exe.parent_path().c_str();info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info))return false;if(info.hProcess)CloseHandle(info.hProcess);return true;
}
void reopen(){if(instance_lock){ReleaseMutex(instance_lock);CloseHandle(instance_lock);instance_lock=nullptr;}auto exe=root/L"RoadRashTrilogyRecomp.exe";auto args=qa?L"--qa-capture "+quote(job/L"updated-launcher.png")+L" --qa-page main":L"";ShellExecuteW(nullptr,L"open",exe.c_str(),args.empty()?nullptr:args.c_str(),root.c_str(),qa?SW_SHOWNOACTIVATE:SW_SHOWNORMAL);DestroyWindow(panel);}
void done(DWORD code){
    busy=false;applying=false;if(worker_job)CloseHandle(worker_job);worker_job=nullptr;
    auto state=value(L"state");
    if(action==L"check"){
        if(code==0&&state==L"available"){offer=value(L"asset");set_status(L"Version "+value(L"version")+L" is available. Download and install it when you are ready.");if(parent)SetWindowTextW(GetDlgItem(parent,207),L"Update available");}
        else if(code==0){offer.clear();set_status(L"You are up to date. Installed launcher: " RR_VERSION L".");}
        else set_status(L"Could not check GitHub. You can keep playing offline and try again later.");
    }else if(action==L"prepare"&&code==0&&state==L"prepared"){
        job=value(L"job");host_exe=value(L"host");
        if(launch_host(false)){DestroyWindow(panel);PostMessageW(parent,WM_CLOSE,0,0);return;}
        set_status(L"Could not open the updater. Your installed games are unchanged.");
    }else if(host_mode){
        if(action==L"cleanup"){can_reopen=true;set_status(cancelled?L"Update cancelled. Your previous version is ready to play.":L"Update could not finish. Your previous version is ready to play.");SetWindowTextW(close_button,L"Open launcher");controls();return;}
        if(code==0){can_reopen=true;set_status(recovering?L"Previous version restored.":L"Update complete.");reopen();return;}
        if(fs::exists(root/L".local/updates/pending.json")&&!recovering){recovering=true;cancelled=false;set_status(L"Restoring the previous version...");start(L"recover");return;}
        if(!fs::exists(root/L".local/updates/pending.json")){set_status(L"Removing temporary build files...");start(L"cleanup");return;}
        can_reopen=!fs::exists(root/L".local/updates/pending.json");set_status(can_reopen?(cancelled?L"Update cancelled. Your previous version is ready to play.":L"Update could not finish. Your previous version is ready to play."):L"Recovery could not finish. Close running games and retry recovery.");
        SetWindowTextW(close_button,can_reopen?L"Open launcher":L"Retry recovery");
    }else set_status(L"Update could not finish. Your installed games are unchanged.");
    if(code){auto error=value(L"error");if(!error.empty())append(error);}controls();
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(rr_updates_event(w,m,wp,lp))return 0;
    if(m==WM_CREATE){
        panel_font=CreateFontW(-(int)(16*dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Barlow");
        auto add=[&](const wchar_t *cls,const wchar_t *text,DWORD style,int x,int y,int width,int height,int id){HWND h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,(int)(x*dpi),(int)(y*dpi),(int)(width*dpi),(int)(height*dpi),w,(HMENU)(INT_PTR)id,app,nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)(panel_font?panel_font:font),TRUE);return h;};
        add(L"STATIC",host_mode?L"Updating Road Rash Trilogy Recomp":L"Road Rash Trilogy updates",0,24,22,650,30,0);
        add(L"STATIC",host_mode?L"Preparing version " RR_VERSION L". Keep this window open until the update finishes.":L"Launcher version " RR_VERSION L". Published test and stable releases are checked on GitHub.",0,24,61,650,44,0);
        status=add(L"STATIC",L"Check for updates to see the latest available version.",0,24,118,650,55,0);
        add(L"STATIC",L"Controls, achievements and ROMs are kept. Campaign checkpoints require their exact original build.",0,24,176,650,42,0);
        details=add(L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL|WS_TABSTOP,24,232,650,142,0);
        check_button=add(L"BUTTON",L"Check for updates",BS_DEFPUSHBUTTON|WS_TABSTOP,24,396,152,38,101);
        download=add(L"BUTTON",L"Download and install",BS_PUSHBUTTON|WS_TABSTOP,188,396,194,38,102);
        add(L"BUTTON",L"Open logs",BS_PUSHBUTTON|WS_TABSTOP,394,396,120,38,103);
        close_button=add(L"BUTTON",host_mode?L"Cancel update":L"Close",BS_PUSHBUTTON|WS_TABSTOP,526,396,148,38,IDCANCEL);controls();return 0;
    }
    if(m==WM_COMMAND){int id=LOWORD(wp);
        if(id==101&&!busy){set_status(L"Checking GitHub...");start(L"check");}
        if(id==102&&!busy&&!offer.empty()){
            if(ready&&!ready()){set_status(L"Close the game or finish installation before updating.");return 0;}
            if(!fs::exists(root/L".local/installation.json")){set_status(L"Automatic installation is available in an installed copy. Use Install games first.");return 0;}
            set_status(L"Downloading and verifying the update...");start(L"prepare");
        }
        if(id==103){auto logs=host_mode?job:root/L".local/updates";fs::create_directories(logs);ShellExecuteW(w,L"open",logs.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
        if(id==IDCANCEL){
            if(host_mode){if(waiting){KillTimer(w,1);DestroyWindow(w);return 0;}if(busy&&!applying){cancelled=true;set_status(L"Stopping update preparation...");EnableWindow(close_button,FALSE);if(worker_job)TerminateJobObject(worker_job,1);}
                else if(!busy){if(can_reopen)reopen();else {recovering=true;start(L"recover");}}}
            else if(!busy||action==L"check")DestroyWindow(w);
            else if(action==L"prepare"){cancelled=true;if(worker_job)TerminateJobObject(worker_job,1);}
        }return 0;
    }
    if(m==WM_TIMER&&host_mode&&!busy){
        if(parent_process&&WaitForSingleObject(parent_process,0)!=WAIT_OBJECT_0){if(++waits>300){KillTimer(w,1);set_status(L"Close the old launcher before continuing the update.");}return 0;}
        DWORD lock=WaitForSingleObject(instance_lock,0);
        if(lock!=WAIT_OBJECT_0&&lock!=WAIT_ABANDONED){if(++waits>300){KillTimer(w,1);set_status(L"Another launcher is open. Close it and reopen Updates.");}return 0;}
        waiting=false;KillTimer(w,1);if(parent_process){CloseHandle(parent_process);parent_process=nullptr;}set_status(recovering?L"Restoring the previous version...":L"Preparing the new version...");start(recovering?L"recover":L"apply");return 0;
    }
    if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLOREDIT){SetTextColor((HDC)wp,RGB(232,234,233));SetBkColor((HDC)wp,RGB(26,29,31));return(LRESULT)brush;}
    if(m==WM_CLOSE){if(!host_mode&&!busy)DestroyWindow(w);else SendMessageW(w,WM_COMMAND,IDCANCEL,0);return 0;}
    if(m==WM_DESTROY){panel=status=details=download=check_button=close_button=nullptr;if(panel_font)DeleteObject(panel_font);panel_font=nullptr;if(parent){EnableWindow(parent,TRUE);if(!qa)SetForegroundWindow(parent);}else PostQuitMessage(0);return 0;}
    return DefWindowProcW(w,m,wp,lp);
}
}
HANDLE rr_updates_mutex(const fs::path& path,bool own){
    std::wstring canonical=fs::absolute(path).lexically_normal().wstring();std::transform(canonical.begin(),canonical.end(),canonical.begin(),towlower);
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;std::wstring name=L"Local\\RoadRashTrilogy_";
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return nullptr;
    if(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){CryptHashData(hash,(const BYTE*)canonical.data(),(DWORD)(canonical.size()*sizeof(wchar_t)),0);BYTE bytes[32];DWORD n=32;if(CryptGetHashParam(hash,HP_HASHVAL,bytes,&n,0))for(auto b:bytes){wchar_t s[3];swprintf_s(s,L"%02x",b);name+=s;}CryptDestroyHash(hash);}CryptReleaseContext(provider,0);
    auto handle=CreateMutexW(nullptr,own,name.c_str());if(own&&GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(handle);return nullptr;}return handle;
}
void rr_updates_init(HINSTANCE a,HFONT f,const fs::path& r,bool q,bool(*allow)()){
    app=a;font=f;root=r;qa=q;ready=allow;python=root/L".tools/python/python.exe";script=root/L"tools/update_release.py";
    if(!brush)brush=CreateSolidBrush(RGB(26,29,31));WNDCLASSW cls{};cls.hInstance=app;cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hbrBackground=brush;cls.lpszClassName=L"RRUpdates";cls.lpfnWndProc=proc;RegisterClassW(&cls);
}
void rr_updates_startup(HWND owner){
    parent=owner;if(qa||!fs::exists(python)||!fs::exists(root/L".local/installation.json"))return;
    auto attempt=root/L".local/updates/check-attempt";std::error_code error;auto stamp=fs::last_write_time(attempt,error);
    if(!error&&fs::file_time_type::clock::now()-stamp<std::chrono::hours(24))return;
    fs::create_directories(attempt.parent_path(),error);if(error)return;std::ofstream(attempt).put('1');start(L"check");
}
void rr_updates_show(HWND owner){
    if(panel){SetForegroundWindow(panel);return;}parent=owner;RECT area{},p{};SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);GetWindowRect(owner,&p);dpi=std::min(GetDpiForWindow(owner)/96.f,(area.bottom-area.top)/500.f);
    EnableWindow(owner,FALSE);panel=CreateWindowExW(WS_EX_DLGMODALFRAME|(qa?WS_EX_NOACTIVATE:0),L"RRUpdates",L"Updates",WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,p.left+(p.right-p.left-(int)(706*dpi))/2,std::max(area.top,p.top+(p.bottom-p.top-(int)(490*dpi))/2),(int)(706*dpi),(int)(490*dpi),owner,nullptr,app,nullptr);
    if(!fs::exists(python))set_status(L"Install games first to enable automatic updates.");else if(!busy&&!qa){set_status(L"Checking GitHub...");start(L"check");}
}
bool rr_updates_event(HWND,UINT m,WPARAM wp,LPARAM lp){
    if(m==LINE){auto line=(std::wstring*)lp;if(line->rfind(L"[APPLY]",0)==0){applying=true;controls();}if(line->rfind(L"[PROGRESS] ",0)==0)set_status(line->substr(11));append(*line);delete line;return true;}
    if(m==DONE){done((DWORD)wp);return true;}return false;
}
bool rr_updates_message(MSG& m){return panel&&IsDialogMessageW(panel,&m);}
HWND rr_updates_window(){return panel;}
void rr_updates_shutdown(){if(worker_job)CloseHandle(worker_job);worker_job=nullptr;if(brush)DeleteObject(brush);brush=nullptr;}
bool rr_updates_recovery(const fs::path& r){
    if(!fs::exists(r/L".local/updates/pending.json"))return false;
    root=r;python=root/L".tools/python/python.exe";script=root/L"tools/update_release.py";result=root/L".local/updates/recovery.ini";
    std::error_code error;fs::remove(result,error);
    auto command=quote(python)+L" -X utf8 "+quote(script)+L" recovery-info --root "+quote(root)+L" --result "+quote(result);
    STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
    if(!CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&si,&pi))return true;
    DWORD wait=WaitForSingleObject(pi.hProcess,30000),exit=1;GetExitCodeProcess(pi.hProcess,&exit);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    job=value(L"job");host_exe=value(L"host");if(wait==WAIT_OBJECT_0&&!exit&&value(L"state")==L"pending"&&launch_host(true))return true;
    MessageBoxW(nullptr,L"An interrupted update needs recovery. Open .local/updates to review the log before playing.",L"Update recovery",MB_OK|MB_ICONERROR);return true;
}
int rr_updates_host(HINSTANCE a,int argc,wchar_t **argv){
    if(argc!=6)return 2;root=fs::absolute(argv[2]);job=fs::absolute(argv[3]);recovering=!wcscmp(argv[1],L"--recover-update");
    DWORD pid=wcstoul(argv[4],nullptr,10);unsigned long long expected=_wcstoui64(argv[5],nullptr,10);parent_process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(parent_process){FILETIME c,e,k,u;ULARGE_INTEGER actual{};if(GetProcessTimes(parent_process,&c,&e,&k,&u)){actual.LowPart=c.dwLowDateTime;actual.HighPart=c.dwHighDateTime;}if(actual.QuadPart!=expected){CloseHandle(parent_process);parent_process=nullptr;}}
    wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);auto package=fs::path(module).parent_path();auto update_root=root;
    RECT area;SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);dpi=std::min(GetDpiForSystem()/96.f,(area.bottom-area.top)/500.f);
    AddFontResourceExW((package/L"assets/launcher/Barlow-Regular.ttf").c_str(),FR_PRIVATE,nullptr);
    font=CreateFontW(-(int)(16*dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Barlow");
    rr_updates_init(a,font,update_root,GetEnvironmentVariableW(L"RR_QA_CAPTURE",nullptr,0)!=0,nullptr);script=package/L"tools/update_release.py";host_mode=true;parent=nullptr;instance_lock=rr_updates_mutex(root,false);
    if(!instance_lock){MessageBoxW(nullptr,L"Could not lock this installation for updating.",L"Update",MB_OK|MB_ICONERROR);return 2;}waiting=true;
    panel=CreateWindowExW(WS_EX_DLGMODALFRAME|(qa?WS_EX_NOACTIVATE:0),L"RRUpdates",L"Road Rash Trilogy Recomp - Update",WS_CAPTION|WS_SYSMENU|WS_VISIBLE,area.left+40,area.top+40,(int)(706*dpi),(int)(490*dpi),nullptr,nullptr,a,nullptr);
    set_status(L"Waiting for the launcher to close...");SetTimer(panel,1,100,nullptr);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){if(!rr_updates_message(m)){TranslateMessage(&m);DispatchMessageW(&m);}}
    if(instance_lock){ReleaseMutex(instance_lock);CloseHandle(instance_lock);}rr_updates_shutdown();DeleteObject(font);return 0;
}
