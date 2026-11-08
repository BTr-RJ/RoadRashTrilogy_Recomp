#pragma once
#include <windows.h>
#include <filesystem>
void rr_updates_init(HINSTANCE app,HFONT font,const std::filesystem::path& root,bool qa,bool (*ready)());
void rr_updates_startup(HWND owner);
void rr_updates_show(HWND owner);
bool rr_updates_event(HWND window,UINT message,WPARAM wp,LPARAM lp);
bool rr_updates_message(MSG& message);
HWND rr_updates_window();
void rr_updates_shutdown();
HANDLE rr_updates_mutex(const std::filesystem::path& root,bool own);
int rr_updates_host(HINSTANCE app,int argc,wchar_t **argv);
bool rr_updates_recovery(const std::filesystem::path& root);
