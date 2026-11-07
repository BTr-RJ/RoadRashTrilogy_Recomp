"""Require byte-identical native PCM with continuous geometry off/on."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone
sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_audio_mix import capture, ROOT

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', choices=['rr1', 'rr2', 'rr3'])
args = parser.parse_args()
output = ROOT / os.environ.get('RR_VIDEO_VALIDATION','build/validation/organic-final') / 'audio' / args.game / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
output.mkdir(parents=True)
hashes = []
for enabled in (0, 1):
    os.environ['RR_GEOMETRY'] = str(enabled)
    wav = capture(args.game, output / str(enabled), 20, 'original')
    hashes.append(hashlib.sha256(wav.read_bytes()).hexdigest())
report = dict(game=args.game, native_pcm_identical=hashes[0] == hashes[1], sha256=hashes)
(output / 'comparison.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2), flush=True)
if not report['native_pcm_identical']:
    raise SystemExit(1)
