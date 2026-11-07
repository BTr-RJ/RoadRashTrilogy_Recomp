"""Verify near-camera growth, complete exits and unchanged Road Rash simulation."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_hill_vehicles import matched


def read_rows(path):
    with path.open(newline='') as source:
        return list(csv.DictReader(source))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'build/validation')
    root = parser.parse_args().root
    final = root / 'vehicle-entry-final'
    report = {'replays': [], 'vehicles': [], 'visible_regressions': {}}
    for game in ('rr1', 'rr2', 'rr3'):
        for scenario, count in (('dynamics', 480), ('touring', 48)):
            after = final / game / 'fixed' / scenario
            before = root / 'vehicle-lanes-final' / game / 'fixed' / scenario
            report['replays'].append(dict(game=game, scenario=scenario,
                                         **matched(before, after, count)))
        data = read_rows(final / game / 'fixed/dynamics/traffic.csv')
        known = [x for x in data if float(x['focal']) > 0 and float(x['display_depth']) > 0]
        raw = [x for x in known if x['world_height'] == '1']
        assert len(raw) > 1000
        assert all(float(x['gain']) == 1 for x in data), 'Arbitrary traffic gain'
        error = max(abs(float(x['scale']) * max(.25, float(x['display_depth'])) -
                        float(x['focal'])) for x in raw)
        assert error < .00001, (game, 'position/scale depth disagreement', error)
        far = [x for x in known if float(x['display_depth']) >= 2.5]
        assert all(float(x['scale']) <= float(x['focal']) / 2.5 + 1e-7 for x in far)
        unknown = [x for x in known if x['world_height'] != '1']
        assert all(float(x['scale']) <= float(x['focal']) / 2.5 + 1e-7 for x in unknown)
        assert all(float(x['scale']) <= float(x['focal']) / .25 + 1e-7 for x in known)
        growing = [x for x in raw if float(x['display_depth']) < 2.5 and
                   float(x['scale']) > float(x['focal']) / 2.5 + .0001]
        assert len(growing) > 10, (game, 'zoom still stops at calibration plane')
        anchored = [x for x in known if x['lane_anchor'] == '1']
        lane_error = max(abs(float(x['x']) - float(x['road_center_x']) -
                             float(x['lane_world']) / float(x['display_depth']) -
                             float(x['body_offset'])) for x in anchored)
        assert lane_error < .002, (game, 'lane projection', lane_error)
        report['vehicles'].append(dict(game=game, raw_ground_observations=len(raw),
            near_growth_observations=len(growing), maximum_perspective_error=error,
            maximum_lane_rounding_error=lane_error, calibration_plane_depth=2.5,
            numerical_near_plane=.25, extra_gain=1))
    approach = final / 'rr1/fixed/approach'
    before = root / 'vehicle-entry-review/rr1/before/approach'
    report['replays'].append(dict(game='rr1', scenario='straight-near-pass',
                                 **matched(before, approach, 240)))

    def coverage(path, actor):
        return {int(x['frame']): int(x['pixels']) for x in read_rows(path / 'vehicle-pixels.csv')
                if x['actor'] == actor}

    rider = coverage(approach, 'FF23AA')
    assert all(rider.get(f, 0) > 0 for f in range(2611, 2636))
    assert all(rider.get(f, 0) == 0 for f in range(2636, 2661))
    car_path = final / 'rr3/fixed/dynamics'
    car = coverage(car_path, 'FF19B2')
    assert all(car.get(f, 0) > 0 for f in range(3830, 3850))
    assert all(car.get(f, 0) == 0 for f in range(3850, 3876))
    trace = read_rows(car_path / 'traffic.csv')
    tops = [float(x['top']) for x in trace if x['actor'] == 'FF19B2' and
            3830 <= int(x['frame']) <= 3852]
    assert len(tops) > 15 and all(b >= a - .0001 for a, b in zip(tops, tops[1:])), 'Growing roof re-entry'
    far = coverage(final / 'rr1/fixed/dynamics', 'FF24AA')
    assert all(far.get(f, 0) > 0 for f in range(3162, 3167)), 'Far-cutoff flicker returned'
    report['visible_regressions'] = dict(rr1_rider_exit_frame=2636,
        rr3_car_exit_frame=3850, whole_car_top_monotone=True,
        rr1_far_cutoff_pixels={f: far[f] for f in range(3162, 3167)})
    for game in ('rr1', 'rr2', 'rr3'):
        assert json.loads((root / 'vehicle-entry-video' / game /
                          'wide-/comparison.json').read_text())['passed']
        audio = json.loads(sorted((root / 'vehicle-entry-video/audio' / game)
                                  .glob('*/comparison.json'))[-1].read_text())
        assert audio['native_pcm_identical']
    assert json.loads((root / 'vehicle-entry-video-43/rr2/comparison.json').read_text())['passed']
    report.update(wide_video_passed=['rr1', 'rr2', 'rr3'], narrow_video_passed=['rr2'],
                  native_pcm_identical=['rr1', 'rr2', 'rr3'])
    workspace = Path(__file__).resolve().parents[1]
    report['executables'] = {g: hashlib.sha256((workspace / 'build/release02/bin' /
        g / (g + '.exe')).read_bytes()).hexdigest() for g in ('rr1', 'rr2', 'rr3')}
    (final / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
