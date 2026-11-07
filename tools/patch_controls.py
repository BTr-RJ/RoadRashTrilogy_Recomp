"""Launcher/runtime controller assignment and conflict-free Road Rash shortcuts."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'.tools/engine/runner'
def edit(name, replacements):
    path=folder/name
    text=path.read_text(encoding='utf-8')
    for old,new in replacements:
        if new in text:
            continue
        if old not in text:
            raise ValueError(f'Input patch context mismatch: {name}')
        text=text.replace(old,new,1)
    if path.read_text(encoding='utf-8')!=text:path.write_text(text,encoding='utf-8')

edit('input_map.h', [('    int         deadzone_pct;',
    '    int         controller_slot; /* physical SDL plug-order slot */\n    int         analog_dpad; /* left stick convenience, 0 disables */\n    int         deadzone_pct;')])
edit('input_map.c', [('    p1->deadzone_pct = 25;',
    '    p1->deadzone_pct = 25; p1->controller_slot=0; p1->analog_dpad=1;'),
    ('    p2->deadzone_pct = 25;', '    p2->deadzone_pct = 25; p2->controller_slot=1; p2->analog_dpad=1;'),
    ('        g_input_map.p[p].device = INPUT_DEV_GAMEPAD;',
     '        g_input_map.p[p].device = INPUT_DEV_GAMEPAD; g_input_map.p[p].controller_slot=p;')])
edit('app_config.c', [('            else if (!strcmp(key, "deadzone")) pi->deadzone_pct = atoi(val);',
    '            else if (!strcmp(key, "deadzone")) pi->deadzone_pct = atoi(val);\n'
    '            else if (!strcmp(key, "controller")) pi->controller_slot = atoi(val);\n'
    '            else if (!strcmp(key, "analog_dpad")) pi->analog_dpad = atoi(val)!=0;'),
    ('    fprintf(f, "deadzone = %d\\n", pi->deadzone_pct);',
     '    fprintf(f, "deadzone = %d\\n", pi->deadzone_pct);\n'
     '    fprintf(f, "controller = %d\\nanalog_dpad = %d\\n",pi->controller_slot,pi->analog_dpad);')])
edit('gamepad.h', [('int gamepad_player_connected(int player);',
    'int gamepad_player_connected(int player);\nint gamepad_controller_instance(int slot);')])
edit('gamepad.c', [('#include <stdio.h>', '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>'),
    ('            if (s_pad[0] && ev->cbutton.which == s_pad_jid[0] &&',
     '            if (!getenv("RR_CONTROLS_PROFILE") && s_pad[0] && ev->cbutton.which == s_pad_jid[0] &&'),
    ('    SDL_GameController *c = s_pad[player];\n    if (!c) return 0;',
     '    int slot=g_input_map.p[player].controller_slot;\n'
     '    SDL_GameController *c = slot>=0&&slot<INPUT_MAX_PLAYERS?s_pad[slot]:NULL;\n    if (!c) return 0;'),
    ('    if (lx < -dz) m |= input_button_bit(GB_LEFT);',
     '    if (pi->analog_dpad && lx < -dz) m |= input_button_bit(GB_LEFT);'),
    ('    if (lx >  dz) m |= input_button_bit(GB_RIGHT);',
     '    if (pi->analog_dpad && lx >  dz) m |= input_button_bit(GB_RIGHT);'),
    ('    if (ly < -dz) m |= input_button_bit(GB_UP);',
     '    if (pi->analog_dpad && ly < -dz) m |= input_button_bit(GB_UP);'),
    ('    if (ly >  dz) m |= input_button_bit(GB_DOWN);',
     '    if (pi->analog_dpad && ly >  dz) m |= input_button_bit(GB_DOWN);'),
    ('    return player >= 0 && player < INPUT_MAX_PLAYERS && s_pad[player] &&\n        SDL_GameControllerGetAttached(s_pad[player]);',
     '    if(player<0||player>=INPUT_MAX_PLAYERS)return 0;\n'
     '    int slot=g_input_map.p[player].controller_slot;\n'
     '    return slot>=0&&slot<INPUT_MAX_PLAYERS&&s_pad[slot]&&SDL_GameControllerGetAttached(s_pad[slot]);'),
    ('int gamepad_turbo_held(void)\n{',
     'int gamepad_controller_instance(int slot){return slot>=0&&slot<INPUT_MAX_PLAYERS?s_pad_jid[slot]:-1;}\n\n'
     'int gamepad_turbo_held(void)\n{\n    if(getenv("RR_CONTROLS_PROFILE"))return 0;')])
