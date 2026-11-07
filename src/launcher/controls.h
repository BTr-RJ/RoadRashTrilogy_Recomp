#pragma once
#include <windows.h>
#include <filesystem>
void rr_controls_init(HINSTANCE instance,HFONT font,const std::filesystem::path& settings);
void rr_controls_show(HWND owner);
bool rr_controls_message(MSG& message);
void rr_controls_write(const std::filesystem::path& game_settings);
unsigned rr_controls_poll(); // left=1, right=2, confirm=4, video=8
void rr_controls_shutdown();
HWND rr_controls_window();
bool rr_controls_self_test(HWND parent);
