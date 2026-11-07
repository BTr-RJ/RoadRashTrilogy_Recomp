"""Export two synchronized native/0.4.3 GIF captures without invented frames."""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.tools/gif-python'))
from PIL import Image

source = ROOT/'build/validation/road043/rr1'
out = ROOT/'build/validation/road043/gifs'
out.mkdir(parents=True, exist_ok=True)
assert json.loads((source/'comparison.json').read_text())['passed']
for name in ('ram.bin','vram.bin'):
    assert (source/'off'/name).read_bytes() == (source/'on'/name).read_bytes()

# GIF uses 10ms ticks. Sample the native 59.94Hz capture at 50fps to keep
# normal speed with 20ms delays, avoiding browser clamping of 10ms frames.
indices = [round(n*120/100) for n in range(100)]
frames = {mode: [Image.open(source/mode/f'{n:03d}.png').convert('RGB') for n in indices]
          for mode in ('off','on')}
colors = set()
for sequence in frames.values():
    for frame in sequence:
        entries = frame.getcolors(65536)
        assert entries is not None
        colors.update(rgb for count,rgb in entries)
assert len(colors) <= 256, 'Use an exact shared Genesis palette for both files'
palette = Image.new('P', (1,1))
flat = [channel for rgb in sorted(colors) for channel in rgb]
palette.putpalette(flat+[0]*(768-len(flat)))
report = dict(game='rr1', version='0.4.3', same_inputs=True, display_aspect='4:3',
              source_frames=120, source_fps=60000/1001, gif_fps=50,
              duration_ms=2000, shared_palette_colors=len(colors), files=[])
for mode,filename in [('off','01-rolagem-original-mega-drive.gif'),
                      ('on','02-zoom-continuo-0.4.3.gif')]:
    sequence = [frame.resize((640,480), Image.Resampling.NEAREST)
                .quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames[mode]]
    target = out/filename
    sequence[0].save(target, save_all=True, append_images=sequence[1:],
                     duration=20, loop=0, disposal=1, optimize=False)
    with Image.open(target) as gif:
        duration = 0
        for n in range(gif.n_frames):
            gif.seek(n)
            duration += gif.info['duration']
        assert gif.size == (640,480) and gif.info['loop'] == 0 and duration == 2000
        # GIF may merge repeated native frames; their accumulated timing stays
        # intact. Verify every displayed source frame at its original time.
        for n,expected in enumerate(sequence):
            gif.seek(0)
            elapsed = 0
            while elapsed+gif.info['duration'] <= n*20:
                elapsed += gif.info['duration']
                gif.seek(gif.tell()+1)
            assert gif.convert('RGB').tobytes() == expected.convert('RGB').tobytes(), (mode,n)
        report['files'].append(dict(name=filename, frames=gif.n_frames, duration_ms=duration,
                                   bytes=target.stat().st_size,
                                   sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    print(target, flush=True)
(out/'comparison.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
