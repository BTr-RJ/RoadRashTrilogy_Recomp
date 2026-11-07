"""Compare consecutive native/continuous frames and deterministic guest state."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import sys
import csv
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read

ROOT = Path(__file__).resolve().parents[1]


def run(game, mode, frames, wait, wide, scenario, baseline):
    output = ROOT / os.environ.get('RR_VIDEO_VALIDATION','build/validation/geometry') / game / ('wide-' if wide else '')
    if scenario != 'cruise':
        output /= scenario
    output /= mode
    output.mkdir(parents=True, exist_ok=True)
    prefix = 'g' + uuid.uuid4().hex[:10]
    names = [f'{prefix}_{n:03d}.png' for n in range(frames)]
    ops = ['WAIT 900', 'SCREENSHOT ' + prefix + '_title.png', 'PRESS START 2',
           'WAIT 240', 'PRESS START 2', 'WAIT 600', 'HOLD B', f'WAIT {wait}']
    if scenario == 'swerve':
        ops += ['HOLD LEFT', 'WAIT 100', 'RELEASE LEFT', 'HOLD RIGHT', 'WAIT 130', 'RELEASE RIGHT', 'WAIT 45']
    for name in names:
        ops += ['SCREENSHOT ' + name, 'WAIT 1']
    ops += [f'DUMP_RAM {prefix}_ram.bin', f'DUMP_VRAM {prefix}_vram.bin',
            'RELEASE', 'WAIT 30', 'PRESS START 2', 'WAIT 90',
            'SCREENSHOT ' + prefix + '_pause.png', 'PRESS START 2', 'WAIT 180',
            f'DUMP_RAM {prefix}_resume-ram.bin', f'DUMP_VRAM {prefix}_resume-vram.bin', 'EXIT']
    script = output / 'drive.input'
    script.write_text('\n'.join(ops) + '\n')
    env = os.environ.copy()
    env.update(RR_ROOT=str(ROOT), RR_MUSIC='original', RR_GEOMETRY='1' if mode == 'on' or baseline=='previous' else '0',
               RR_GEOMETRY_STATS=str(output / 'geometry.json'),
               RR_GEOMETRY_TRACE=str(output / 'geometry.csv'), RR_WIDE='1' if wide else '0',
               RR_BILLBOARD_TRACE=str(output / 'objects.csv'),
               RR_TEST_HIDDEN='1', RR_OUTPUT_WIDTH='960', RR_OUTPUT_HEIGHT='540',
               RR_SHADER_PATH='', GENESIS_SCREEN='raw', SDL_RENDER_VSYNC='0', SDL_AUDIODRIVER='dummy')
    if baseline=='previous':env['RR_ORGANIC']='1' if mode=='on' else '0'
    exe = ROOT / 'build/release02/bin' / game / (game + '.exe')
    with (output / 'run.log').open('w') as log:
        subprocess.run([str(exe), str(ROOT / '.local/build-inputs' / game / 'game.bin'),
                        '--no-launcher', '--benchmark', str(wait + frames + 2700), '--pacing',
                        'accurate', '--input-script', str(script)], env=env, cwd=output,
                       stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW,
                       check=True, timeout=120)
    for i, source in enumerate(names):
        shutil.move(str(exe.parent / source), str(output / f'{i:03d}.png'))
    for suffix in ['title.png', 'pause.png', 'ram.bin', 'vram.bin', 'resume-ram.bin', 'resume-vram.bin']:
        shutil.move(str(exe.parent / (prefix + '_' + suffix)), str(output / suffix))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', choices=['rr1', 'rr2', 'rr3'])
    parser.add_argument('--frames', type=int, default=120)
    parser.add_argument('--wait', type=int, default=600)
    parser.add_argument('--wide', action='store_true')
    parser.add_argument('--scenario', choices=['cruise', 'swerve'], default='cruise')
    parser.add_argument('--baseline', choices=['native', 'previous'], default='native')
    args = parser.parse_args()
    off = run(args.game, 'off', args.frames, args.wait, args.wide, args.scenario, args.baseline)
    on = run(args.game, 'on', args.frames, args.wait, args.wide, args.scenario, args.baseline)
    def digest(path):
        return hashlib.sha256(path.read_bytes()).digest()
    before = [digest(off / f'{n:03d}.png') for n in range(args.frames)]
    after = [digest(on / f'{n:03d}.png') for n in range(args.frames)]
    extra = sum(before[i] == before[i-1] and after[i] != after[i-1] for i in range(1, args.frames))
    unchanged = {name: (off / name).read_bytes() == (on / name).read_bytes()
                 for name in ['ram.bin', 'vram.bin', 'resume-ram.bin', 'resume-vram.bin', 'title.png', 'pause.png']}
    hud_unchanged = True
    for n in range(args.frames):
        a, b = read(off / f'{n:03d}.png'), read(on / f'{n:03d}.png')
        hud_unchanged &= a[:3] == b[:3] and a[3][152:] == b[3][152:]
    width = read(on / '000.png')[0]
    with (on / 'geometry.csv').open() as source:
        scale_samples = sum(abs(float(row['scale']) - float(row['native_scale'])) > 1e-7
                            for row in csv.DictReader(source))
    report = dict(game=args.game, frames=args.frames, wide=args.wide, scenario=args.scenario,
                  baseline=args.baseline,
                  source_width=width, hud_unchanged=hud_unchanged,
                  intermediate_scale_samples=scale_samples,
                  native_distinct=len(set(before)), geometry_distinct=len(set(after)),
                  additional_motion_frames=extra, unchanged=unchanged,
                  stats=json.loads((on / 'geometry.json').read_text()))
    report['passed'] = all(unchanged.values()) and hud_unchanged and width == (426 if args.wide else 320)
    if args.scenario == 'cruise':
        report['passed'] &= extra >= (1 if args.baseline=='previous' else 10) and scale_samples > 100 and report['geometry_distinct'] > report['native_distinct']
    (on.parent / 'comparison.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2), flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
