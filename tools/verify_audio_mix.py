"""Verify real mixed PCM across two pauses, with and without the remake.

Uses independent output folders; never repairs an unfinished WAV header or
accepts logs alone as proof of audible music. Run with the pinned Python:
    .tools/python/python.exe tools/verify_audio_mix.py rr1
"""
import argparse
import array
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import wave
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
FPS = 60000 / 1001


def capture(game, output, volume, soundtrack='dean-harris'):
    output.mkdir(parents=True)
    script = output / 'drive.input'
    address, choice = {'rr1': (0xff05d6, 1), 'rr2': (0xfff594, 1),
                       'rr3': (0xff752c, 0)}[game]
    restored = f'ASSERT_RAM16 {address:06X} {choice:04X}\n'
    script.write_text(
        'WAIT 900\nPRESS START 12\nWAIT 240\nPRESS START 12\n'
        'WAIT 600\nHOLD B\nWAIT 400\nRELEASE\nWAIT 30\n' + restored +
        'PRESS START 12\nWAIT 120\n' + restored +
        'PRESS START 12\nWAIT 180\n' + restored +
        'PRESS START 12\nWAIT 120\n' + restored +
        'PRESS START 12\nWAIT 180\n' + restored + 'EXIT\n',
        encoding='utf-8')
    env = os.environ.copy()
    env.update(RR_ROOT=str(ROOT), RR_MUSIC=soundtrack,
               RR_MUSIC_VOLUME=str(volume), RR_EFFECTS_VOLUME='100',
               RR_TEST_HIDDEN='1', RR_OUTPUT_WIDTH='960', RR_OUTPUT_HEIGHT='540',
               RR_WIDE='1', RR_SHADER_PATH='', GENESIS_SCREEN='raw',
               SDL_AUDIODRIVER='dummy', SDL_RENDER_VSYNC='0')
    exe = ROOT / 'build/release02/bin' / game / (game + '.exe')
    wav = output / 'mix.wav'
    with (output / 'run.log').open('w', encoding='utf-8') as log:
        subprocess.run(
            [str(exe), str(ROOT / '.local/build-inputs' / game / 'game.bin'),
             '--no-launcher', '--turbo', '--max-frames', '3100',
             '--pacing', 'accurate', '--input-script', str(script), '--wav', str(wav)],
            cwd=output, env=env, stdout=log, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW, timeout=100, check=True)
    text = (output / 'run.log').read_text(encoding='utf-8', errors='replace')
    if 'parse error' in text or '[audio] WAV capture stopped:' not in text:
        raise ValueError('Replay failed or PCM capture was not finalized')
    return wav


def samples(reader, start, end):
    first = round(start / FPS * reader.getframerate())
    count = round((end - start) / FPS * reader.getframerate())
    reader.setpos(first)
    data = reader.readframes(count)
    if len(data) != count * 4:
        raise ValueError('PCM capture is truncated')
    values = array.array('h', data)
    if sys.byteorder != 'little':
        values.byteswap()
    return values


def signal_rms(values):
    # A constant chip offset is silence. Remove each channel's DC separately.
    powers = []
    for channel in (values[::2], values[1::2]):
        mean = sum(channel) / len(channel)
        powers.append(sum((x - mean) ** 2 for x in channel) / len(channel))
    return math.sqrt(sum(powers) / 2)


def verify(mixed_path, effects_path):
    windows = [('running', 2030, 2090), ('paused_1', 2210, 2260),
               ('resumed_1', 2350, 2400), ('paused_2', 2510, 2560),
               ('resumed_2', 2650, 2700)]
    with wave.open(str(mixed_path), 'rb') as mixed, wave.open(str(effects_path), 'rb') as fx:
        if mixed.getparams() != fx.getparams():
            raise ValueError('The paired captures have different PCM formats or lengths')
        if mixed.getnchannels() != 2 or mixed.getsampwidth() != 2 or mixed.getnframes() == 0:
            raise ValueError('Expected a finalized, nonempty stereo PCM16 WAV')
        report = dict(sample_rate=mixed.getframerate(), frames=mixed.getnframes(), windows=[], errors=[])
        for name, start, end in windows:
            a, b = samples(mixed, start, end), samples(fx, start, end)
            residual = [int(x) - int(y) for x, y in zip(a, b)]
            music, effects = signal_rms(residual), signal_rms(b)
            clip = sum(x in (-32768, 32767) for x in a) / len(a)
            if name.startswith('paused'):
                if music > 0.01:
                    report['errors'].append(f'{name}: remake still contributes PCM during pause ({music})')
            elif music <= 5:
                report['errors'].append(f'{name}: no audible remake contribution ({music})')
            if not name.startswith('paused') and effects <= 5:
                report['errors'].append(f'{name}: native effects are silent')
            if clip > 0.001:
                report['errors'].append(f'{name}: more than 0.1% of samples are clipped')
            report['windows'].append(dict(name=name, music_rms=round(music, 3),
                                          effects_rms=round(effects, 3),
                                          clipped_fraction=clip))
        report['passed'] = not report['errors']
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', choices=['rr1', 'rr2', 'rr3'])
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    output = ROOT / 'build/validation/audio-pcm' / args.game / stamp
    exe = ROOT / 'build/release02/bin' / args.game / (args.game + '.exe')
    digest = hashlib.sha256(exe.read_bytes()).hexdigest()
    print(f'{args.game}: capturing remake at 20%, effects at 100%', flush=True)
    mixed = capture(args.game, output / 'mixed', 20)
    print(f'{args.game}: capturing the identical replay with remake muted', flush=True)
    fx = capture(args.game, output / 'effects', 0)
    if hashlib.sha256(exe.read_bytes()).hexdigest() != digest:
        raise ValueError('Executable changed during paired captures; rerun verification')
    report = verify(mixed, fx)
    report.update(game=args.game, executable_sha256=digest, music_volume=20,
                  effects_volume=100, guest_option_restored=True)
    (output / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)
    print(f'Evidence: {output}', flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
