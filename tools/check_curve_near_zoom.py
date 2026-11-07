"""Verify the high-curve clamp and near-camera scale-reset regressions."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from png_utils import read


def compare(before, after, count):
    files = sorted(before.glob('[0-9][0-9][0-9].png'))
    assert len(files) == count, (before, len(files))
    for f in files:
        for suffix in ('.ram', '.vram'):
            assert f.with_suffix(suffix).read_bytes() == (after / f.name).with_suffix(suffix).read_bytes(), (after, f.name, suffix)
        a, b = read(f), read(after / f.name)
        assert a[:3] == b[:3] and a[3][152:] == b[3][152:], (after, f.name, 'HUD')
    return dict(frames=count, identical_guest_snapshots=count * 2, identical_hud_frames=count)


def raster(folder):
    result = json.loads((folder / 'raster-audit.csv.json').read_text())
    assert result['player_pixels'] > 100000
    assert all(result[k] == 0 for k in ('corrupted_player_pixels', 'missing_actor_pixels', 'blue_road_pixels')), (folder, result)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1] / 'build/validation')
    a = p.parse_args(); r = a.root; report = {'replays': []}
    for game in ('rr1', 'rr2'):
        for kind, scenario, count in (('road-curve', 'touring', 48), ('near-zoom', 'approach', 240)):
            before = r / (kind + '-review') / game / 'before' / scenario
            after = r / (kind + '-final') / game / 'fixed' / scenario
            report['replays'].append(dict(game=game, scenario=scenario, **compare(before, after, count), raster=raster(after)))
    for kind, game, scenario in (('road-curve', 'rr1', 'corner'), ('near-zoom', 'rr3', 'approach')):
        base = r / (kind + '-final') / game
        report['replays'].append(dict(game=game, scenario=scenario, **compare(base / 'native' / scenario, base / 'fixed' / scenario, 240), raster=raster(base / 'fixed' / scenario)))
    right = r / 'right-curve-final/rr1'
    report['replays'].append(dict(game='rr1', scenario='offroad-left',
        **compare(right / 'native/approach', right / 'fixed/approach', 240), raster=raster(right / 'fixed/approach')))

    def edge_pixels(path):
        w, h, c, rows = read(path)
        # The opposite, diagonal edge legitimately crosses x=0 at rows 90-98.
        # Below row 100 the complete road has left the view; the old vertical
        # stripe incorrectly remained there to the HUD.
        return sum(rows[y][x*c] == rows[y][x*c+1] == rows[y][x*c+2] and rows[y][x*c] > 80
                   for y in range(100, 152) for x in range(53))
    before = edge_pixels(r / 'road-curve-review/rr1/before/touring/032.png')
    after = edge_pixels(r / 'road-curve-final/rr1/fixed/touring/032.png')
    assert before > 200 and after == 0, ('vertical clipped edge persists', before, after)
    report['rr1_vertical_edge'] = dict(before_pixels=before, after_pixels=after)
    def right_edge(path):
        w, h, c, rows = read(path)
        return sum(rows[y][x*c] == rows[y][x*c+1] == rows[y][x*c+2] and rows[y][x*c] > 80
                   for y in range(110, 152) for x in range(350, 426))
    old_right = right_edge(right / 'native/approach/100.png')
    new_right = right_edge(right / 'fixed/approach/100.png')
    assert old_right > 200 and new_right == 0
    report['rr1_right_vertical_edge'] = dict(before_pixels=old_right, after_pixels=new_right)

    def close_scales(path):
        with path.open() as f:
            return [float(x['scale']) for x in csv.DictReader(f) if x['actor'] == 'FF1A62' and 2682 <= int(x['frame']) <= 2685]
    old = close_scales(r / 'near-zoom-review/rr2/before/approach/traffic.csv')
    new = close_scales(r / 'near-zoom-final/rr2/fixed/approach/traffic.csv')
    assert len(old) == len(new) == 4 and old[2] < old[1] * .6
    assert all(b > a for a, b in zip(new, new[1:])), ('near scale still resets', new)
    report['rr2_near_scale'] = dict(before=old, after=new)
    for game in ('rr1', 'rr2', 'rr3'):
        video = json.loads((r / 'curve-zoom-final' / game / 'wide-/comparison.json').read_text())
        assert video['passed'], video
        audio = json.loads(sorted((r / 'curve-zoom-final/audio' / game).glob('*/comparison.json'))[-1].read_text())
        assert audio['native_pcm_identical'], audio
    assert json.loads((r / 'curve-zoom-final-43/rr2/comparison.json').read_text())['passed']
    report.update(wide_video_passed=['rr1', 'rr2', 'rr3'], narrow_video_passed=['rr2'], identical_native_pcm=['rr1', 'rr2', 'rr3'])
    root = Path(__file__).resolve().parents[1]
    report['executables'] = {g: hashlib.sha256((root / 'build/release02/bin' / g / (g + '.exe')).read_bytes()).hexdigest() for g in ('rr1', 'rr2', 'rr3')}
    (r / 'curve-zoom-final/result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
