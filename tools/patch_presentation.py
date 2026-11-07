from pathlib import Path
import difflib
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'.tools/engine/runner/main.c'
old=p.read_text(encoding='utf-8')
new=old
if 'RR_GPU_PRESENT' not in old:
    new=new.replace('#include "audio.h"','#include "audio.h"\n#include "gpu_present.h" /* RR_GPU_PRESENT */')
    new=new.replace('        h = display_logical_height();','        h = display_logical_height();\n        /* MD artwork targets a 4:3 CRT: PAR 14:15, never fill-stretch. */\n        w *= 14; h *= 15;')
    new=new.replace('        SDL_RenderPresent(renderer);\n#if GENESIS_HAS_RECOMP_NET',
        '        rr_gpu_present(renderer,present_src,s_frame_stride,s_screen_width,s_screen_height,frame_num);\n        SDL_RenderPresent(renderer);\n#if GENESIS_HAS_RECOMP_NET')
    new=new.replace('    SDL_DestroyRenderer(renderer);','    rr_gpu_shutdown();\n    SDL_DestroyRenderer(renderer);')
    p.write_text(new,encoding='utf-8')
old=p.read_text(encoding='utf-8')
if 'RR_MOTION_PRESENT' not in old:
    new=old.replace('#include "gpu_present.h" /* RR_GPU_PRESENT */', '#include "gpu_present.h" /* RR_GPU_PRESENT */\n#include "motion_bridge.h" /* RR_MOTION_PRESENT */')
    new=new.replace('        const uint32_t *present_src = s_framebuf;',
                    '        const uint32_t *present_src = rr_motion_present(s_framebuf,s_frame_stride,s_screen_width,s_screen_height,frame_num);')
    new=new.replace('color_lut_map_frame(&s_color_lut, s_framebuf, s_present_buf,','color_lut_map_frame(&s_color_lut, present_src, s_present_buf,')
    p.write_text(new,encoding='utf-8')
old=p.read_text(encoding='utf-8')
if 'RR_PRESENT_PROBE' not in old:
    new=old.replace('        if (!benchmark_frames) {\n        const uint32_t *present_src',
                    '        if (!benchmark_frames || getenv("RR_PRESENT_PROBE")) {\n        const uint32_t *present_src')
    p.write_text(new,encoding='utf-8')
    (ROOT/'patches/engine-presentation.patch').write_text(''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/runner/main.c',tofile='b/runner/main.c')),encoding='utf-8')
print('Applied aspect-preserving presentation and native Slang hook')
old=p.read_text(encoding='utf-8')
if 'RR_SCRIPT_ERRORS' not in old:
    new=old.replace('    if (input_script_path) input_script_load(input_script_path);',
        '    /* RR_SCRIPT_ERRORS: an invalid explicit replay must not run attract mode. */\n'
        '    if (input_script_path && !input_script_load(input_script_path)) return 2;')
    new=new.replace('                return input_script_exit_code();',
        '                audio_wav_stop(); /* finalize PCM evidence on scripted exit */\n                return input_script_exit_code();')
    p.write_text(new,encoding='utf-8')
old=p.read_text(encoding='utf-8')
new=old.replace('rr_gpu_present(renderer,present_src,s_frame_stride,s_screen_width,s_screen_height,frame_num)',
    'rr_gpu_present(renderer,present_src,s_frame_stride,s_screen_width,s_screen_height,gvdp_screen_width(&g_machine.vdp),frame_num)')
new=new.replace('        w *= 14; h *= 15;',
    '        /* RR_NATIVE_ASPECT: H32 and H40 both target 4:3. */\n'
    '        w *= 4 * gvdp_output_height(&g_machine.vdp);\n'
    '        h *= 3 * gvdp_screen_width(&g_machine.vdp);')
p.write_text(new,encoding='utf-8')
old=p.read_text(encoding='utf-8')
if 'RR_CAPTURE_PRESENT' not in old:
    new=old.replace('        SDL_RenderPresent(renderer);\n#if GENESIS_HAS_RECOMP_NET',
        '        rr_present_capture(renderer,frame_num); /* RR_CAPTURE_PRESENT */\n        SDL_RenderPresent(renderer);\n#if GENESIS_HAS_RECOMP_NET')
    new=new.replace('int test_hidden = getenv("LNG_SCRIPT") && getenv("LNG_TEST_HIDDEN");',
        'int test_hidden = getenv("RR_TEST_HIDDEN") || (getenv("LNG_SCRIPT") && getenv("LNG_TEST_HIDDEN"));')
    new=new.replace('    if (!benchmark_frames) {\n        if (g_app_config.fullscreen == 2)',
        '    if (!benchmark_frames && !test_hidden) {\n        if (g_app_config.fullscreen == 2)')
    p.write_text(new,encoding='utf-8')
old=p.read_text(encoding='utf-8')
if 'RR_GAME_KEYS' not in old:
    new=old.replace('            if (ev.type == SDL_KEYDOWN) {',
        '            if (ev.type == SDL_KEYDOWN) {\n                extern int rr_game_key(int); /* RR_GAME_KEYS */\n                if(!ev.key.repeat && rr_game_key((int)ev.key.keysym.sym)) continue;')
    p.write_text(new,encoding='utf-8')
