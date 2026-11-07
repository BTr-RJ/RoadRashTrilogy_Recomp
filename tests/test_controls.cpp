#define NOMINMAX
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
extern "C" {
#include "input_map.h"
#include "app_config.h"
#include "gamepad.h"
}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"controls line %d: %s (%s)\n",__LINE__,#x,SDL_GetError());return 1;}}while(0)
static void poll(){SDL_JoystickUpdate();SDL_Event e;while(SDL_PollEvent(&e))gamepad_handle_event(&e);}
int main(){
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    CHECK(SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)==0);
    int first=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);
    int second=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);
    CHECK(first>=0&&second>=0);
    SDL_Joystick *one=SDL_JoystickOpen(first),*two=SDL_JoystickOpen(second);CHECK(one&&two);
    input_map_init_defaults();app_config_defaults();gamepad_init_players(6);
    int slot_one=-1,slot_two=-1;
    for(int i=0;i<6;++i){if(gamepad_controller_instance(i)==SDL_JoystickInstanceID(one))slot_one=i;if(gamepad_controller_instance(i)==SDL_JoystickInstanceID(two))slot_two=i;}
    CHECK(slot_one>=0&&slot_two>=0);
    auto& p=g_input_map.p[0];p.device=INPUT_DEV_BOTH;p.controller_slot=slot_one;p.analog_dpad=0;
    p.key[GB_B]=SDL_SCANCODE_G;
    p.pad[GB_B]={GP_BIND_BUTTON,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0};
    p.pad[GB_A]={GP_BIND_AXIS,SDL_CONTROLLER_AXIS_RIGHTX,-1};
    auto path=std::filesystem::temp_directory_path()/"rr-controls-roundtrip.ini";
    CHECK(app_config_save(path.string().c_str()));
    input_map_init_defaults();CHECK(app_config_load(path.string().c_str()));
    CHECK(g_input_map.p[0].key[GB_B]==SDL_SCANCODE_G&&g_input_map.p[0].controller_slot==slot_one&&g_input_map.p[0].analog_dpad==0);
    CHECK(g_input_map.p[0].pad[GB_A].kind==GP_BIND_AXIS&&g_input_map.p[0].pad[GB_A].axis_dir==-1);
    // gamepad.c reads the application's MSVC CRT environment. SDL2's DLL
    // may have a separate CRT copy, so SDL_setenv alone is insufficient here.
    CHECK(_putenv_s("RR_CONTROLS_PROFILE","1")==0);
    CHECK(SDL_JoystickSetVirtualButton(one,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1)==0);poll();
    CHECK(gamepad_player_mask(0)&input_button_bit(GB_B));
    CHECK(!gamepad_consume_quickload()); // remapped shoulder must not load a snapshot
    CHECK(SDL_JoystickSetVirtualAxis(one,SDL_CONTROLLER_AXIS_RIGHTX,-30000)==0);poll();
    CHECK(gamepad_player_mask(0)&input_button_bit(GB_A));
    CHECK(SDL_JoystickSetVirtualAxis(one,SDL_CONTROLLER_AXIS_LEFTX,30000)==0);poll();
    CHECK(!(gamepad_player_mask(0)&input_button_bit(GB_RIGHT)));
    g_input_map.p[0].analog_dpad=1;CHECK(gamepad_player_mask(0)&input_button_bit(GB_RIGHT));
    g_input_map.p[0].controller_slot=slot_two;CHECK(!(gamepad_player_mask(0)&input_button_bit(GB_B)));
    CHECK(gamepad_player_connected(0));
    gamepad_shutdown();SDL_JoystickClose(one);SDL_JoystickClose(two);SDL_JoystickDetachVirtual(second);SDL_JoystickDetachVirtual(first);SDL_Quit();
    std::filesystem::remove(path);
    std::puts("Controls: key persistence, button/axis remaps, gamepad assignment, optional analog and no shortcut collisions passed");
}
