"""Verify rival pose ownership, bounded lane correction and unchanged simulation."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
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
    final = root / 'owner-rivals-complete'
    report = {'replays': [], 'vehicles': []}
    for game in ('rr1', 'rr2', 'rr3'):
        for label in ('fixed', 'fixed-late'):
            old_label = 'before' if label == 'fixed' else 'before-late'
            old = root / 'owner-rivals-review' / game / old_label / 'rivals'
            new = final / game / label / 'rivals'
            report['replays'].append(dict(game=game, scenario='combat-' + label,
                                         **matched(old, new, 480)))
        for scenario, count in (('dynamics', 480), ('touring', 48)):
            old = root / 'vehicle-entry-final' / game / 'fixed' / scenario
            new = final / game / 'fixed' / scenario
            report['replays'].append(dict(game=game, scenario=scenario,
                                         **matched(old, new, count)))
        trace = []
        for path in (final / game).glob('*/*/traffic.csv'):
            trace.extend(rows(path))
        lane = [x for x in trace if x['lane_anchor'] == '1' and float(x['display_depth']) > 0]
        assert len(lane) > 1000
        correction = [abs(float(x['lane_world']) - float(x['lane_target'])) /
                      float(x['display_depth']) for x in lane]
        assert max(correction) <= 8.0001, (game, 'lane correction bound', max(correction))
        corrected = sum(d > .01 for d in correction)
        assert corrected > 100, (game, 'lane correction not exercised')
        error = max(abs(float(x['x']) - float(x['road_center_x']) -
                        float(x['lane_world']) / float(x['display_depth']) -
                        float(x['body_offset'])) for x in lane)
        assert error < .002
        for x in trace:
            assert all(math.isfinite(float(x[k])) for k in ('x', 'y', 'scale'))
            assert float(x['scale']) > 0 and float(x['gain']) == 1
        raw = [x for x in lane if x['world_height'] == '1' and float(x['focal']) > 0]
        perspective_error = max(abs(float(x['scale']) * max(.25, float(x['display_depth'])) -
                                    float(x['focal'])) for x in raw)
        assert perspective_error < .00001, (game, 'near perspective changed')
        report['vehicles'].append(dict(game=game, lane_observations=len(lane),
            corrected_lane_observations=corrected, maximum_correction_pixels=max(correction),
            maximum_lane_rounding_error=error, maximum_perspective_error=perspective_error,
            live_mid_gap_observations=sum(x['live_gap'] == '1' for x in trace)))
    new = final / 'rr1/fixed/approach'
    old = root / 'vehicle-entry-final/rr1/fixed/approach'
    report['replays'].append(dict(game='rr1', scenario='near-pass', **matched(old, new, 240)))
    far = {int(x['frame']): int(x['pixels']) for x in
           rows(final / 'rr1/fixed/dynamics/vehicle-pixels.csv') if x['actor'] == 'FF24AA'}
    assert all(far.get(f, 0) > 0 for f in range(3162, 3167)), 'Far-cutoff regression'
    car = {int(x['frame']): int(x['pixels']) for x in
           rows(final / 'rr3/fixed/dynamics/vehicle-pixels.csv') if x['actor'] == 'FF19B2'}
    assert all(car.get(f, 0) == 0 for f in range(3860, 3876)), 'Mirror re-entry regression'
    for game in ('rr1', 'rr2', 'rr3'):
        assert json.loads((root / 'owner-rivals-final-video' / game /
                          'wide-/comparison.json').read_text())['passed']
        audio = json.loads(sorted((root / 'owner-rivals-final-video/audio' / game)
                                  .glob('*/comparison.json'))[-1].read_text())
        assert audio['native_pcm_identical']
    assert json.loads((root / 'owner-rivals-final-video-43/rr2/comparison.json').read_text())['passed']
    workspace = Path(__file__).resolve().parents[1]
    native_log = (workspace / 'build/rivals-native-tests.log').read_text(encoding='utf-16')
    assert '100% tests passed out of 9' in native_log
    old_log = (workspace / 'build/rivals-old-regression.log').read_text(encoding='utf-16')
    assert 'std::abs(center()-205.5)<.6' in old_log
    report.update(native_groups=9, previous_engine_failed_reversal_fixture=True,
        wide_video_passed=['rr1', 'rr2', 'rr3'], narrow_video_passed=['rr2'],
        native_pcm_identical=['rr1', 'rr2', 'rr3'],
        verification_scope='paired native replays plus synthetic transition fixtures; owner video is a visual reference, not an exact input replay')
    report['executables'] = {g: hashlib.sha256((workspace / 'build/release02/bin' /
        g / (g + '.exe')).read_bytes()).hexdigest() for g in ('rr1', 'rr2', 'rr3')}
    report['previous_engine_source_sha256'] = hashlib.sha256((workspace /
        'dist/0.50-approval-entry/RoadRashTrilogyRecomp-0.50-source/src/presentation/sprite_geometry.cpp').read_bytes()).hexdigest()
    (final / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
