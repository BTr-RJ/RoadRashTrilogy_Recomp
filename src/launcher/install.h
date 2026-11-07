#pragma once
#include <windows.h>
#include <filesystem>
void rr_install_init(HINSTANCE instance,HFONT font,const std::filesystem::path& package);
void rr_install_show(HWND owner);
bool rr_install_message(MSG& message);
bool rr_install_busy();
HWND rr_install_window();
