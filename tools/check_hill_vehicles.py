"""Verify vehicle depth, player-relative size, hills and unchanged guest timing."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read


def matched(before, after, count, minimum_player_pixels=100000):
    for i in range(count):
        for suffix in ('ram', 'vram'):
            name = f'{i:03d}.{suffix}'
            assert (before / name).read_bytes() == (after / name).read_bytes(), (after, name)
        a, b = read(before / f'{i:03d}.png'), read(after / f'{i:03d}.png')
        assert a[:3] == b[:3] and a[3][152:] == b[3][152:], (after, i, 'HUD changed')
    audit = json.loads((after / 'raster-audit.csv.json').read_text())
    assert audit['player_pixels'] > minimum_player_pixels
    assert all(audit[k] == 0 for k in ('corrupted_player_pixels', 'missing_actor_pixels', 'blue_road_pixels')), audit
    return dict(frames=count, identical_guest_snapshots=count * 2, identical_hud_frames=count, raster=audit)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1] / 'build/validation')
    args = p.parse_args(); r = args.root; report = {'replays': [], 'vehicles': []}
    for game in ('rr1', 'rr2', 'rr3'):
        new = r / 'hill-vehicle-confirmed' / game / 'fixed/touring'
        old = r / 'hill-vehicle-review' / game / 'before/touring'
        report['replays'].append(dict(game=game, scenario='touring', **matched(old, new, 48)))
        with (new / 'traffic.csv').open() as f:
            rows = list(csv.DictReader(f))
        assert all(float(x['gain']) == 1 for x in rows), (game, 'extra enlargement remains')
        known = [x for x in rows if float(x['focal']) > 0]
        physical = [x for x in known if float(x['display_depth']) > 0]
        anchored = [x for x in physical if x['hill_anchor'] == '1']
        assert len(physical) > 1000 and len(anchored) > 1000
        ratios = [float(x['scale']) / (float(x['focal']) / 2.5) for x in known]
        assert max(ratios) <= 1.0000001, (game, 'oversized vehicle', max(ratios))
        last = {}; continuous = 0
        for x in physical:
            key = x['actor']; a = last.get(key)
            if a and int(a['frame']) == int(x['frame']) - 1 and a['source_tick'] == x['source_tick'] and a['focal'] == x['focal']:
                depth_a, depth_b = float(a['display_depth']), float(x['display_depth'])
                sa, sb = float(a['scale']), float(x['scale'])
                if depth_b < depth_a - 1e-6:
                    assert sb + 1e-7 >= sa, (game, x['frame'], 'approach shrinks between updates')
                continuous += abs(sb - sa) > 1e-7
            last[key] = x
        assert continuous > 100
        report['vehicles'].append(dict(game=game, calibrated_observations=len(known),
            physical_depth_observations=len(physical), hill_anchored_observations=len(anchored),
            continuous_scale_steps=continuous, maximum_player_plane_ratio=max(ratios), extra_gain=1))
    for game in ('rr1', 'rr2', 'rr3'):
        before = r / 'near-zoom-final' / game / 'fixed/approach'
        after = r / 'hill-vehicle-confirmed' / game / 'fixed/approach'
        report['replays'].append(dict(game=game, scenario='fast-approach', **matched(before, after, 240)))
    for game in ('rr1', 'rr2', 'rr3'):
        assert json.loads((r / 'hill-vehicle-final-video' / game / 'wide-/comparison.json').read_text())['passed']
        audio = json.loads(sorted((r / 'hill-vehicle-final-video/audio' / game).glob('*/comparison.json'))[-1].read_text())
        assert audio['native_pcm_identical'], audio
    assert json.loads((r / 'hill-vehicle-final-video-43/rr2/comparison.json').read_text())['passed']
    report.update(wide_video_passed=['rr1', 'rr2', 'rr3'], narrow_video_passed=['rr2'], native_pcm_identical=['rr1', 'rr2', 'rr3'])
    root = Path(__file__).resolve().parents[1]
    report['executables'] = {g: hashlib.sha256((root / 'build/release02/bin' / g / (g + '.exe')).read_bytes()).hexdigest() for g in ('rr1', 'rr2', 'rr3')}
    (r / 'hill-vehicle-confirmed/result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
