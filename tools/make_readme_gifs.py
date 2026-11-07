"""Export synchronized native/0.50 GIFs from qualified local video pairs.

Uses Pillow from .tools/gif-python or the current Python environment.
Raw captures and guest dumps remain private; only the named GIFs and their
small provenance manifest are written into the public media directory.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools/gif-python'))
from PIL import Image, ImageDraw, ImageFont

SOURCE_FPS = 60000 / 1001
GIF_FPS = 50
BACKGROUND = (16, 19, 21)
INK = (244, 240, 226)
ACCENT = (255, 201, 37)
SIZE = (960, 396)
PANEL = (480, 360)
TITLES = {'rr1': 'ROAD RASH', 'rr2': 'ROAD RASH II', 'rr3': 'ROAD RASH 3'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def export(game, source, output, expected):
    pair = json.loads((source / 'comparison.json').read_text())
    assert pair['passed'] and pair['baseline'] == 'native' and not pair['wide']
    assert pair['hud_unchanged'] and all(pair['unchanged'].values())
    for filename in ('ram.bin', 'vram.bin', 'resume-ram.bin', 'resume-vram.bin'):
        assert (source / 'off' / filename).read_bytes() == (source / 'on' / filename).read_bytes()
    executable = ROOT / f'build/release02/bin/{game}/{game}.exe'
    executable_hash = digest(executable)
    assert executable_hash == expected[f'build/release02/bin/{game}/{game}.exe']

    count = math.floor(pair['frames'] / SOURCE_FPS * GIF_FPS)
    indices = [round(n * SOURCE_FPS / GIF_FPS) for n in range(count)]
    frames = {}
    colors = {BACKGROUND, INK, ACCENT}
    for mode in ('off', 'on'):
        frames[mode] = []
        for index in indices:
            with Image.open(source / mode / f'{index:03d}.png') as image:
                frame = image.convert('RGB')
            assert frame.size == (320, 224)
            entries = frame.getcolors(65536)
            assert entries is not None
            colors.update(rgb for _, rgb in entries)
            frames[mode].append(frame)
    assert len(colors) <= 256, 'Exact shared Mega Drive palette exceeds GIF capacity'
    palette = Image.new('P', (1, 1))
    flat = [channel for rgb in sorted(colors) for channel in rgb]
    palette.putpalette(flat + [0] * (768 - len(flat)))
    font = ImageFont.truetype(str(ROOT / 'assets/launcher/BarlowCondensed-Bold.ttf'), 23)
    sequence = []
    for left, right in zip(frames['off'], frames['on']):
        frame = Image.new('RGB', SIZE, BACKGROUND)
        frame.paste(left.resize(PANEL, Image.Resampling.NEAREST), (0, 36))
        frame.paste(right.resize(PANEL, Image.Resampling.NEAREST), (480, 36))
        draw = ImageDraw.Draw(frame)
        draw.fontmode = '1'  # Keep the game's exact palette; no text antialias colors.
        draw.text((12, 4), 'ORIGINAL MEGA DRIVE', font=font, fill=INK)
        draw.text((492, 4), 'VERSION 0.50 / CONTINUOUS ZOOM', font=font, fill=ACCENT)
        indexed = frame.quantize(palette=palette, dither=Image.Dither.NONE)
        assert indexed.convert('RGB').tobytes() == frame.tobytes()
        sequence.append(indexed)
    target = output / f'{game}-original-vs-zoom-0.50.gif'
    sequence[0].save(target, save_all=True, append_images=sequence[1:],
                     duration=20, loop=0, disposal=1, optimize=False)
    # Verify every displayed pair against its intended original frame time.
    with Image.open(target) as gif:
        assert gif.size == SIZE and gif.info['loop'] == 0
        decoded, durations = [], []
        for n in range(gif.n_frames):
            gif.seek(n)
            decoded.append(gif.convert('RGB'))
            durations.append(gif.info['duration'])
        assert sum(durations) == count * 20
        shown = elapsed = 0
        for n, intended in enumerate(sequence):
            while elapsed + durations[shown] <= n * 20:
                elapsed += durations[shown]
                shown += 1
            assert decoded[shown].tobytes() == intended.convert('RGB').tobytes()
    assert target.stat().st_size < 10 * 1024 * 1024, 'Keep each README GIF below 10 MiB'
    # Inspection sheet is private, outside the public output directory.
    sheet = Image.new('RGB', (960, 396 * 3), BACKGROUND)
    for row, n in enumerate((0, count // 2, count - 1)):
        sheet.paste(sequence[n].convert('RGB'), (0, row * 396))
    sheet.save(source / 'readme-review.png')
    script = (source / 'on/drive.input').read_text().splitlines()
    wait = int(script[script.index('HOLD B') + 1].split()[1])
    first_frame = int(re.search(r'SCREENSHOT requested at frame (\d+) \(path=\w+_000\.png\)',
                                (source / 'on/run.log').read_text()).group(1))
    return dict(game=game, title=TITLES[game], file=target.name,
                bytes=target.stat().st_size, sha256=digest(target),
                executable_sha256=executable_hash, source_frames=pair['frames'],
                initial_racing_wait=wait, first_source_frame=first_frame,
                gif_frames=count, duration_ms=count * 20, shared_palette_colors=len(colors),
                ram_vram_match=True, hud_match=True, gif_pixels_verified=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-build', type=Path, required=True,
                        help='Private validation JSON containing executable SHA-256 values')
    args = parser.parse_args()
    expected = json.loads(args.expected_build.read_text())['executables']
    args.output.mkdir(parents=True, exist_ok=True)
    files = [export(game, args.input / game, args.output, expected)
             for game in ('rr1', 'rr2', 'rr3')]
    report = dict(version='0.50', display_aspect='4:3', source_fps=SOURCE_FPS,
                  gif_fps=GIF_FPS, left='Native Mega Drive raster in the project runtime',
                  right='Version 0.50 continuous geometry and sprite zoom',
                  synchronized=True, normal_speed=True, files=files)
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n',
                                                encoding='utf-8', newline='\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
