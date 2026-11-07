"""Verify native vehicle anchors at model adoption and unchanged guest execution."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_hill_vehicles import matched
from check_rival_transitions import rows
from png_utils import read


def grid_samples(path):
    data = rows(path)
    first = int(data[0]['frame'])
    return [x for x in data if int(x['frame']) < first + 16
            and float(x['source_camera']) > 0]


def anchor_errors(data):
    return {axis: max(abs(float(x[axis]) - float(x['source_' + axis]))
                      for x in data) for axis in ('x', 'y')}


def rider_mask(path):
    w, h, channels, pixels = read(path)
    offset = (w - 320) // 2
    result = set()
    for y in range(100, 140):
        for x in range(76 + offset, 136 + offset):
            r, g, b = pixels[y][x * channels:x * channels + 3]
            if (b > r + 50 and b > g + 50) or (r > 100 and g > 100 and b < 100):
                result.add((x, y))
    return result


def main():
    workspace = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=workspace / 'build/validation')
    root = parser.parse_args().root
    final = root / 'grid-final'
    report = dict(replays=[], grid=[], vehicles=[])
    for game in ('rr1', 'rr2', 'rr3'):
        before = root / 'grid-review' / game / 'before/grid'
        after = final / game / 'fixed/grid'
        # RR1 includes its loading/countdown interval before player rendering.
        report['replays'].append(dict(game=game, scenario='grid',
            **matched(before, after, 480, minimum_player_pixels=80000)))
        old, new = grid_samples(before / 'traffic.csv'), grid_samples(after / 'traffic.csv')
        assert len(old) == len(new) == 112, (game, 'grid not exercised')
        old_errors, new_errors = anchor_errors(old), anchor_errors(new)
        assert old_errors['x'] > (10 if game == 'rr1' else 20)
        assert max(new_errors.values()) < .001, (game, new_errors)
        report['grid'].append(dict(game=game, observations=len(new),
            previous_maximum_anchor_error=old_errors, maximum_anchor_error=new_errors))
        for scenario, label, count in [('rivals', 'fixed', 480), ('rivals', 'fixed-late', 480),
                                      ('dynamics', 'fixed', 480), ('touring', 'fixed', 48)]:
            before = root / 'owner-rivals-complete' / game / label / scenario
            after = final / game / label / scenario
            report['replays'].append(dict(game=game, scenario=scenario + '-' + label,
                                         **matched(before, after, count)))
        trace = []
        for path in (final / game).glob('*/*/traffic.csv'):
            trace.extend(rows(path))
        for x in trace:
            assert all(math.isfinite(float(x[k])) for k in ('x', 'y', 'scale'))
            assert float(x['scale']) > 0 and float(x['gain']) == 1
        lane = [x for x in trace if x['lane_anchor'] == '1' and float(x['display_depth']) > 0]
        assert len(lane) > 1000
        correction = max(abs(float(x['lane_world']) - float(x['lane_target'])) /
                         float(x['display_depth']) for x in lane)
        assert correction <= 8.0001, (game, 'lane filter bound', correction)
        projection_error = max(abs(float(x['x']) - float(x['road_center_x']) -
            float(x['lane_world']) / float(x['display_depth']) - float(x['body_offset'])) for x in lane)
        assert projection_error < .002
        raw = [x for x in lane if x['world_height'] == '1' and float(x['focal']) > 0]
        perspective_error = max(abs(float(x['scale']) * max(.25, float(x['display_depth'])) -
                                    float(x['focal'])) for x in raw)
        assert perspective_error < .00001, (game, 'near scale', perspective_error)
        report['vehicles'].append(dict(game=game, lane_observations=len(lane),
            maximum_lane_correction_pixels=correction, maximum_lane_rounding_error=projection_error,
            maximum_perspective_error=perspective_error))
    report['replays'].append(dict(game='rr1', scenario='near-pass', **matched(
        root / 'owner-rivals-complete/rr1/fixed/approach', final / 'rr1/fixed/approach', 240)))
    native = rider_mask(root / 'grid-review/rr1/native/grid/195.png')
    similarities = []
    for path in (root / 'grid-review/rr1/before/grid/195.png', final / 'rr1/fixed/grid/195.png'):
        pixels = rider_mask(path)
        similarities.append(len(native & pixels) / len(native | pixels))
    assert similarities[0] < .1 and similarities[1] > .8, similarities
    report['rr1_grid_rider_native_mask_iou'] = dict(previous=similarities[0], current=similarities[1])
    far = {int(x['frame']): int(x['pixels']) for x in rows(final /
        'rr1/fixed/dynamics/vehicle-pixels.csv') if x['actor'] == 'FF24AA'}
    assert all(far.get(f, 0) > 0 for f in range(3162, 3167)), 'Far-cutoff regression'
    car = {int(x['frame']): int(x['pixels']) for x in rows(final /
        'rr3/fixed/dynamics/vehicle-pixels.csv') if x['actor'] == 'FF19B2'}
    assert all(car.get(f, 0) == 0 for f in range(3860, 3876)), 'Mirror re-entry regression'
    for game in ('rr1', 'rr2', 'rr3'):
        assert json.loads((root / 'grid-final-video' / game / 'wide-/comparison.json').read_text())['passed']
        audio = json.loads(sorted((root / 'grid-final-video/audio' / game).glob('*/comparison.json'))[-1].read_text())
        assert audio['native_pcm_identical']
    assert json.loads((root / 'grid-final-video-43/rr2/comparison.json').read_text())['passed']
    assert '100% tests passed out of 9' in (workspace / 'build/grid-native-tests.log').read_text(encoding='utf-16')
    assert 'Ran 24 tests' in (workspace / 'build/grid-python-tests.log').read_text()
    assert 'std::abs(center()-155.5)<.6' in (workspace / 'build/grid-old-regression.log').read_text(encoding='utf-16')
    report.update(native_groups=9, python_tests=24, previous_engine_failed_grid_fixture=True,
        wide_video_passed=['rr1', 'rr2', 'rr3'], narrow_video_passed=['rr2'],
        native_pcm_identical=['rr1', 'rr2', 'rr3'],
        verification_scope='scripted grid/steering/combat/hill replays and pixel fixtures; owner video is a visual reference, not an exact input replay')
    report['executables'] = {g: hashlib.sha256((workspace / 'build/release02/bin' / g /
        (g + '.exe')).read_bytes()).hexdigest() for g in ('rr1', 'rr2', 'rr3')}
    report['previous_engine_source_sha256'] = hashlib.sha256((workspace /
        'dist/0.50-approval-rivals/RoadRashTrilogyRecomp-0.50-source/src/presentation/sprite_geometry.cpp').read_bytes()).hexdigest()
    (final / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
