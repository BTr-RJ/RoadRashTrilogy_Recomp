"""Check wide source recovery, continuous actor motion and unchanged guest state."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read


def poses(path):
    result = {}
    with path.open() as f:
        for row in csv.DictReader(f):
            result.setdefault((int(row['frame']), row['actor']), row)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--clipped-fixed', type=Path, required=True)
    p.add_argument('--after', type=Path, required=True)
    p.add_argument('--four-three', type=Path, required=True)
    a = p.parse_args()
    report = {'games': []}
    for game in ('rr1', 'rr2', 'rr3'):
        old = a.before / game / 'before/traffic'
        stepped = a.clipped_fixed / game / 'fixed/traffic'
        new = a.after / game / 'final/traffic'
        states = sorted(old.glob('*.ram')) + sorted(old.glob('*.vram'))
        assert len(states) == 960, (game, 'incomplete state capture')
        assert all(f.read_bytes() == (new / f.name).read_bytes() for f in states), (game, 'guest changed')
        images = sorted(old.glob('[0-9][0-9][0-9].png'))
        assert len(images) == 480
        for f in images:
            before, after = read(f), read(new / f.name)
            assert before[:3] == after[:3] and before[3][152:] == after[3][152:], (game, f.name, 'HUD changed')
        stats = json.loads((new / 'geometry.json').read_text())
        assert stats['actor_restored_pixels'] > 0 and stats['actor_source_only'] > 0, (game, 'no edge recovery')
        previous, current = poses(stepped / 'traffic.csv'), poses(new / 'traffic.csv')
        extra = 0
        for key, row in current.items():
            earlier = (key[0] - 1, key[1])
            if earlier not in current or key not in previous or earlier not in previous:
                continue
            def scale(rows, k): return float(rows[k]['scale'])
            if abs(scale(previous, key) - scale(previous, earlier)) < 1e-7 and abs(scale(current, key) - scale(current, earlier)) > 1e-7:
                extra += 1
        assert extra > 100, (game, 'no additional continuous actor motion', extra)
        video = json.loads((a.after / game / 'wide-/comparison.json').read_text())
        assert video['passed'], (game, 'native/geometry video comparison')
        audio_files = sorted((a.after / 'audio' / game).glob('*/comparison.json'))
        assert audio_files, (game, 'missing audio comparison')
        audio = json.loads(audio_files[-1].read_text())
        assert audio['native_pcm_identical'], (game, 'audio timing changed')
        report['games'].append(dict(game=game, identical_guest_snapshots=len(states),
            identical_hud_frames=len(images), extra_actor_motion_frames=extra,
            restored_source_pixels=stats['actor_restored_pixels'], source_only_assemblies=stats['actor_source_only'],
            native_video_passed=True, native_pcm_identical=True))
    narrow = json.loads((a.four_three / 'rr2/comparison.json').read_text())
    assert narrow['passed']
    report['rr2_four_three_passed'] = True
    root = Path(__file__).resolve().parents[1]
    report['executables'] = {g: hashlib.sha256((root / 'build/release02/bin' / g / (g + '.exe')).read_bytes()).hexdigest() for g in ('rr1', 'rr2', 'rr3')}
    (a.after / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
