"""Verify curve-relative vehicles, live cutoff recovery and one-way passes."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_hill_vehicles import matched


def rows(path):
    with path.open(newline='') as source:
        return list(csv.DictReader(source))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'build/validation')
    root = parser.parse_args().root
    report = {'replays': [], 'vehicles': [], 'visible_regressions': {}}
    for game in ('rr1', 'rr2', 'rr3'):
        for scenario, count in (('dynamics', 480), ('touring', 48)):
            new = root / 'vehicle-lanes-final' / game / 'fixed' / scenario
            before = (root / 'vehicle-lanes-review' / game / 'before' / scenario
                      if scenario == 'dynamics' else
                      root / 'hill-vehicle-confirmed' / game / 'fixed' / scenario)
            report['replays'].append(dict(game=game, scenario=scenario,
                                         **matched(before, new, count)))
        traffic = rows(root / 'vehicle-lanes-final' / game / 'fixed/dynamics/traffic.csv')
        assert all(float(x['gain']) == 1 for x in traffic), (game, 'extra size gain')
        known = [x for x in traffic if float(x['focal']) > 0]
        anchored = [x for x in known if x['lane_anchor'] == '1' and float(x['display_depth']) > 0]
        assert len(anchored) > 1000
        ratios = [float(x['scale']) / (float(x['focal']) / 2.5) for x in known]
        assert max(ratios) <= 1.0000001, (game, 'player-plane size bound')
        errors = [abs(float(x['x']) - (float(x['road_center_x']) +
                  float(x['lane_world']) / float(x['display_depth']) +
                  float(x['body_offset']))) for x in anchored]
        assert max(errors) < .002, (game, 'curve-relative lane projection', max(errors))
        raw = sum(x['world_height'] == '1' for x in anchored)
        assert raw > 1000, (game, 'unclipped world-ground projection')
        report['vehicles'].append(dict(game=game, calibrated_observations=len(known),
            lane_anchored_observations=len(anchored), raw_ground_observations=raw,
            maximum_lane_rounding_error=max(errors), maximum_player_plane_ratio=max(ratios),
            far_hold_observations=sum(x['far_hold'] == '1' for x in traffic),
            near_hold_observations=sum(x['near_hold'] == '1' for x in traffic)))

    pixels = rows(root / 'vehicle-lanes-final/rr1/fixed/dynamics/vehicle-pixels.csv')
    far = {int(x['frame']): int(x['pixels']) for x in pixels if x['actor'] == 'FF24AA'}
    assert all(far.get(frame, 0) > 0 for frame in range(3162, 3167)), 'RR1 far-cutoff blink'
    report['visible_regressions']['rr1_far_cutoff'] = {f: far[f] for f in range(3162, 3167)}
    pixels = rows(root / 'vehicle-lanes-final/rr3/fixed/dynamics/vehicle-pixels.csv')
    car = {int(x['frame']): int(x['pixels']) for x in pixels if x['actor'] == 'FF19B2'}
    assert all(car.get(frame, 0) > 0 for frame in range(3830, 3837)), 'RR3 incomplete pass'
    assert all(car.get(frame, 0) == 0 for frame in range(3837, 3856)), 'RR3 mirror re-entry'
    report['visible_regressions']['rr3_car_pass'] = {f: car.get(f, 0) for f in range(3830, 3856)}

    for game in ('rr1', 'rr2', 'rr3'):
        assert json.loads((root / 'vehicle-lanes-final-video' / game /
                          'wide-/comparison.json').read_text())['passed']
        audio = json.loads(sorted((root / 'vehicle-lanes-final-video/audio' / game)
                                  .glob('*/comparison.json'))[-1].read_text())
        assert audio['native_pcm_identical'], audio
    assert json.loads((root / 'vehicle-lanes-final-video-43/rr2/comparison.json').read_text())['passed']
    report.update(wide_video_passed=['rr1', 'rr2', 'rr3'], narrow_video_passed=['rr2'],
                  native_pcm_identical=['rr1', 'rr2', 'rr3'])
    workspace = Path(__file__).resolve().parents[1]
    report['executables'] = {game: hashlib.sha256((workspace / 'build/release02/bin' /
        game / (game + '.exe')).read_bytes()).hexdigest() for game in ('rr1', 'rr2', 'rr3')}
    target = root / 'vehicle-lanes-final/result.json'
    target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
