#define NOMINMAX
#include "install.h"
#include <commctrl.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <array>
#include <vector>
#include <fstream>
#include <string>
#include <thread>
#include <algorithm>
namespace {
HINSTANCE app;HFONT font;HBRUSH brush;HWND panel,owner,destination,logbox,progress,status,install_button,close_button;
std::array<HWND,3> folders;std::vector<HWND> edits;
std::filesystem::path package,target;
HANDLE worker_job=nullptr;bool running=false,cancelled=false,complete=false;float dpi=1;
constexpr UINT LOG_LINE=WM_APP+61,DONE=WM_APP+62;
std::wstring widen(const std::string& text){int n=MultiByteToWideChar(CP_UTF8,0,text.data(),(int)text.size(),nullptr,0);std::wstring result(n,L'\0');MultiByteToWideChar(CP_UTF8,0,text.data(),(int)text.size(),result.data(),n);return result;}
std::wstring value(HWND c){int n=GetWindowTextLengthW(c);std::wstring s(n+1,L'\0');GetWindowTextW(c,s.data(),n+1);s.resize(n);return s;}
std::wstring quote(const std::wstring& input){std::wstring result=L"\"";unsigned slashes=0;for(wchar_t c:input){if(c==L'\\'){++slashes;continue;}if(c==L'\"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}result.append(slashes*2,L'\\');return result+L"\"";}
std::wstring pick_folder(HWND parent,const wchar_t* title){
    IFileOpenDialog *dialog=nullptr;std::wstring result;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){
        DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);dialog->SetTitle(title);
        if(SUCCEEDED(dialog->Show(parent))){IShellItem *item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result=path;CoTaskMemFree(path);}item->Release();}}
        dialog->Release();
    }
    return result;
}
void append(const std::wstring& s){int n=GetWindowTextLengthW(logbox);SendMessageW(logbox,EM_SETSEL,n,n);auto line=s+L"\r\n";SendMessageW(logbox,EM_REPLACESEL,FALSE,(LPARAM)line.c_str());SendMessageW(logbox,EM_SCROLLCARET,0,0);}
void stop(){if(running&&worker_job){cancelled=true;TerminateJobObject(worker_job,130);SetWindowTextW(status,L"Stopping installation. Your original ROM folders will be kept.");EnableWindow(close_button,FALSE);}}
void begin(){
    if(running)return;
    std::wstring roms[3];bool supplied=false;for(int i=0;i<3;++i){roms[i]=value(folders[i]);supplied|=!roms[i].empty();}
    if(!supplied){SetWindowTextW(status,L"Select at least one folder containing your compatible ROMs.");return;}
    target=value(destination);if(target.empty()){SetWindowTextW(status,L"Choose a new empty installation folder.");return;}
    auto script=package/L"scripts/install.ps1";if(!std::filesystem::exists(script)){SetWindowTextW(status,L"Installer files are missing. Extract the complete release ZIP and try again.");return;}
    wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);auto powershell=std::filesystem::path(system)/L"WindowsPowerShell/v1.0/powershell.exe";
    std::wstring command=quote(powershell.wstring())+L" -NoProfile -ExecutionPolicy Bypass -File "+quote(script.wstring())+L" -Destination "+quote(target.wstring());
    for(int i=0;i<3;++i)if(!roms[i].empty())command+=L" -RomFolder"+std::to_wstring(i+1)+L" "+quote(roms[i]);
    SECURITY_ATTRIBUTES attributes{sizeof attributes,nullptr,TRUE};HANDLE read=nullptr,write=nullptr;
    if(!CreatePipe(&read,&write,&attributes,0)){SetWindowTextW(status,L"Could not create the installation log pipe.");return;}
    SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
    HANDLE null_input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr);
    STARTUPINFOW start{};start.cb=sizeof start;start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=start.hStdError=write;start.hStdInput=null_input;
    PROCESS_INFORMATION process{};worker_job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool job_ok=worker_job&&SetInformationJobObject(worker_job,JobObjectExtendedLimitInformation,&limit,sizeof limit);
    bool created=job_ok&&CreateProcessW(powershell.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,package.c_str(),&start,&process);
    CloseHandle(write);if(null_input!=INVALID_HANDLE_VALUE)CloseHandle(null_input);
    if(!created){CloseHandle(read);if(worker_job)CloseHandle(worker_job);worker_job=nullptr;SetWindowTextW(status,L"Could not start PowerShell. Check permissions and extract the full package.");return;}
    if(!AssignProcessToJobObject(worker_job,process.hProcess)){TerminateProcess(process.hProcess,1);CloseHandle(process.hProcess);CloseHandle(process.hThread);CloseHandle(read);CloseHandle(worker_job);worker_job=nullptr;SetWindowTextW(status,L"Could not create a cancellable installation process.");return;}
    running=true;cancelled=false;complete=false;for(HWND c:edits)EnableWindow(c,FALSE);EnableWindow(install_button,FALSE);SetWindowTextW(close_button,L"Stop installation");SetWindowTextW(logbox,L"");SendMessageW(progress,PBM_SETPOS,0,0);SetWindowTextW(status,L"Installing from your ROMs. Microsoft C++ tools may request administrator approval.");
    ResumeThread(process.hThread);CloseHandle(process.hThread);HWND receiver=panel;auto log_path=package/L".local/install-0.50.log";
    std::thread([read,handle=process.hProcess,receiver,log_path]{
        std::ofstream file(log_path,std::ios::binary|std::ios::app);std::string pending;char bytes[4096];DWORD count;
        while(ReadFile(read,bytes,sizeof bytes,&count,nullptr)&&count){file.write(bytes,count);file.flush();pending.append(bytes,count);size_t at;while((at=pending.find('\n'))!=std::string::npos){auto line=pending.substr(0,at);pending.erase(0,at+1);if(!line.empty()&&line.back()=='\r')line.pop_back();auto text=new std::wstring(widen(line));if(!PostMessageW(receiver,LOG_LINE,0,(LPARAM)text))delete text;}}
        if(!pending.empty()){auto text=new std::wstring(widen(pending));if(!PostMessageW(receiver,LOG_LINE,0,(LPARAM)text))delete text;}
        CloseHandle(read);WaitForSingleObject(handle,INFINITE);DWORD exit=1;GetExitCodeProcess(handle,&exit);CloseHandle(handle);PostMessageW(receiver,DONE,exit,0);
    }).detach();
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    if(m==WM_CREATE){
        font=CreateFontW(-(int)(16*dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Barlow");
        auto add=[&](const wchar_t* cls,const wchar_t* caption,DWORD style,int x,int y,int width,int height,int id){HWND c=CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,(int)(x*dpi),(int)(y*dpi),(int)(width*dpi),(int)(height*dpi),w,(HMENU)(INT_PTR)id,app,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;};
        add(L"STATIC",L"Install from your own ROMs",0,24,20,700,32,0);
        add(L"STATIC",L"Choose one or more ROM folders. Compatible BIN, MD, GEN and ZIP files are detected automatically.\nOnly games you provide will be installed. Original ROM folders are never changed.",0,24,58,710,55,0);
        const wchar_t* titles[]={L"Road Rash / ROM folder",L"Road Rash II / ROM folder",L"Road Rash 3 / ROM folder"};
        for(int i=0;i<3;++i){add(L"STATIC",titles[i],0,24,126+i*62,710,24,0);folders[i]=add(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,24,151+i*62,592,29,10+i);auto browse=add(L"BUTTON",L"Browse...",BS_PUSHBUTTON|WS_TABSTOP,628,151+i*62,106,29,20+i);edits.push_back(folders[i]);edits.push_back(browse);}
        add(L"STATIC",L"Installation folder",0,24,316,710,24,0);
        wchar_t local[MAX_PATH]{};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local);auto default_path=std::filesystem::path(local)/L"Programs/RoadRashTrilogyRecomp";
        destination=add(L"EDIT",default_path.c_str(),WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,24,342,592,29,13);edits.push_back(destination);edits.push_back(add(L"BUTTON",L"Browse...",BS_PUSHBUTTON|WS_TABSTOP,628,342,106,29,23));
        progress=add(PROGRESS_CLASSW,L"",PBS_SMOOTH,24,390,710,18,0);SendMessageW(progress,PBM_SETRANGE,0,MAKELPARAM(0,100));
        status=add(L"STATIC",L"Internet is needed for missing tools. The games are generated and compiled on your PC.",0,24,418,710,44,0);
        logbox=add(L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL|WS_TABSTOP,24,472,710,156,30);
        add(L"BUTTON",L"Open logs",BS_PUSHBUTTON|WS_TABSTOP,24,644,126,36,40);
        install_button=add(L"BUTTON",L"Install games",BS_DEFPUSHBUTTON|WS_TABSTOP,436,644,140,36,IDOK);close_button=add(L"BUTTON",L"Close",BS_PUSHBUTTON|WS_TABSTOP,590,644,144,36,IDCANCEL);return 0;
    }
    if(m==WM_COMMAND){int id=LOWORD(wp);
        if(id>=20&&id<=23){auto folder=pick_folder(w,id==23?L"Choose installation parent folder":L"Choose a folder containing your ROMs");if(!folder.empty()){if(id==23)folder=(std::filesystem::path(folder)/L"RoadRashTrilogyRecomp").wstring();SetWindowTextW(id==23?destination:folders[id-20],folder.c_str());}return 0;}
        if(id==40){ShellExecuteW(w,L"open",(package/L".local").c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;}
        if(id==IDOK){
            if(complete){auto exe=target/L"RoadRashTrilogyRecomp.exe";ShellExecuteW(w,L"open",exe.c_str(),nullptr,target.c_str(),SW_SHOWNORMAL);DestroyWindow(w);}
            else begin();return 0;
        }
        if(id==IDCANCEL){if(running)stop();else DestroyWindow(w);return 0;}
    }
    if(m==LOG_LINE){auto text=(std::wstring*)lp;append(*text);if(text->rfind(L"[STAGE ",0)==0&&text->size()>8){int stage=(*text)[7]-L'0';SendMessageW(progress,PBM_SETPOS,std::clamp((stage-1)*100/7,0,95),0);auto end=text->find(L"] ");SetWindowTextW(status,text->substr(end==std::wstring::npos?0:end+2).c_str());}delete text;return 0;}
    if(m==DONE){running=false;if(worker_job)CloseHandle(worker_job);worker_job=nullptr;for(HWND c:edits)EnableWindow(c,TRUE);EnableWindow(install_button,TRUE);EnableWindow(close_button,TRUE);SetWindowTextW(close_button,L"Close");complete=wp==0&&!cancelled;
        if(complete){SendMessageW(progress,PBM_SETPOS,100,0);SetWindowTextW(status,L"Installation complete. Your games are ready in the installed launcher.");SetWindowTextW(install_button,L"Open launcher");}
        else SetWindowTextW(status,cancelled?L"Installation stopped. Your original ROMs are unchanged. You can retry in this installation folder.":L"Installation failed. Read the log above, correct the problem and retry. Your original ROMs are unchanged.");return 0;
    }
    if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLOREDIT){SetTextColor((HDC)wp,RGB(232,234,233));SetBkColor((HDC)wp,RGB(26,29,31));return(LRESULT)brush;}
    if(m==WM_CLOSE){if(running)stop();else DestroyWindow(w);return 0;}
    if(m==WM_DESTROY){panel=nullptr;edits.clear();DeleteObject(font);font=nullptr;EnableWindow(owner,TRUE);if(!GetEnvironmentVariableW(L"RR_QA_CAPTURE",nullptr,0))SetForegroundWindow(owner);return 0;}
    return DefWindowProcW(w,m,wp,lp);
}
}
void rr_install_init(HINSTANCE instance,HFONT body,const std::filesystem::path& root){app=instance;font=body;package=root;brush=CreateSolidBrush(RGB(26,29,31));WNDCLASSW klass{};klass.hInstance=app;klass.hCursor=LoadCursor(nullptr,IDC_ARROW);klass.hbrBackground=brush;klass.lpszClassName=L"RRInstaller";klass.lpfnWndProc=proc;RegisterClassW(&klass);}
void rr_install_show(HWND parent){if(panel){SetForegroundWindow(panel);return;}owner=parent;complete=false;RECT work{},r{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);GetWindowRect(parent,&r);dpi=std::min(GetDpiForWindow(parent)/96.f,(work.bottom-work.top)/738.f);int width=(int)(782*dpi),height=(int)(732*dpi);EnableWindow(parent,FALSE);panel=CreateWindowExW(WS_EX_DLGMODALFRAME|(GetEnvironmentVariableW(L"RR_QA_CAPTURE",nullptr,0)?WS_EX_NOACTIVATE:0),L"RRInstaller",L"Install Road Rash Trilogy Recomp 0.50",WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,r.left+(r.right-r.left-width)/2,std::max(work.top,r.top+(r.bottom-r.top-height)/2),width,height,parent,nullptr,app,nullptr);}
bool rr_install_message(MSG& message){return panel&&IsDialogMessageW(panel,&message);}
bool rr_install_busy(){return running;}
HWND rr_install_window(){return panel;}
