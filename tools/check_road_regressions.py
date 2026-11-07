"""Check the recorded 0.4.3 regressions, unchanged RR3, video/PCM and launcher."""
from pathlib import Path
import hashlib
import json
import os
import re
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read

ROOT = Path(__file__).resolve().parents[1]
out = ROOT/os.environ.get('RR_VISUAL_VALIDATION', 'build/validation/road043')
version = re.search(r'project\(RoadRashTrilogyRecomp VERSION ([\d.]+)', (ROOT/'CMakeLists.txt').read_text()).group(1)
assert version == '0.4.3', 'Use captures and binaries from 0.4.3 for this regression package'
report = dict(version=version, raster=[], presentation=[], audio=[], launcher=[])
for game, scenes in [('rr1', ['combat', 'corner', 'corner-right', 'touring', 'traffic']),
                     ('rr2', ['combat', 'corner', 'corner-right', 'touring']),
                     ('rr3', ['touring'])]:
    for scene in scenes:
        folder = out/game/'fixed'/scene
        audit = json.loads((folder/'pixel-audit.json').read_text())
        assert audit['passed'], (game, scene, audit)
        assert 'version=0.4.3' in (folder/'run.log').read_text(errors='replace')
        report['raster'].append(audit)

def paint_rows(path):
    w, h, c, rows = read(path)
    result = []
    for y in range(136,152):
        xs = [x for x in range(230,300) if rows[y][x*c:x*c+3] == bytes((206,206,0))]
        runs = sum(i == 0 or x != xs[i-1]+1 for i,x in enumerate(xs))
        result.append(dict(line=y, runs=runs, pixels=len(xs)))
    return result

before = paint_rows(out/'rr2/before/corner/080.png')
after = paint_rows(out/'rr2/fixed/corner/080.png')
assert any(r['runs'] > 1 for r in before), 'The baseline must reproduce the bifurcation'
assert all(r['runs'] == 1 and 1 <= r['pixels'] <= 8 for r in after), after
report['rr2_paint_regression'] = dict(before=before, after=after, passed=True)

for n in range(48):
    for ext in ('png','ram','vram'):
        name = f'{n:03d}.{ext}'
        assert (ROOT/'build/validation/approach042/rr3/fixed/touring'/name).read_bytes() == (out/'rr3/fixed/touring'/name).read_bytes(), name
report['rr3_unchanged'] = dict(frames=48, identical_files=144, passed=True)

for game in ('rr1','rr2'):
    for folder in (out/game, out/game/'wide-', out/game/'wide-/swerve'):
        video = json.loads((folder/'comparison.json').read_text())
        assert video['passed'], video
        report['presentation'].append(video)
for game in ('rr1','rr2','rr3'):
    pcm = json.loads(sorted((out/'audio'/game).glob('*/comparison.json'))[-1].read_text())
    assert pcm['native_pcm_identical'], pcm
    report['audio'].append(pcm)
    launcher = json.loads((out/'launcher'/game/'result.json').read_text())
    assert launcher['exit'] == 0 and launcher['crt'] and launcher['preferences_unchanged']
    report['launcher'].append(launcher)
report['executables'] = {game: hashlib.sha256((ROOT/'build/release02/bin'/game/(game+'.exe')).read_bytes()).hexdigest() for game in ('rr1','rr2','rr3')}
report['passed'] = True
(out/'result.json').write_text(json.dumps(report, indent=2))
print(f"0.4.3 passed: {len(report['raster'])} raster replays, 6 video pairs, 3 PCM pairs, 3 CRT launches")
print('RR2: bifurcated paint replaced by one stroke on all 16 regression rows')
print('RR3: 48 screenshots and their RAM/VRAM unchanged')
