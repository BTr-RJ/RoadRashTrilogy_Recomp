"""Check the production launcher -> game environment, logs and actual CRT output."""
from pathlib import Path
import os,subprocess,json,argparse,re
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--game',choices=['rr1','rr2','rr3'],default='rr1');args=p.parse_args()
version=re.search(r'project\(RoadRashTrilogyRecomp VERSION ([\d.]+)',(ROOT/'CMakeLists.txt').read_text()).group(1)
out=ROOT/os.environ.get('RR_VIDEO_VALIDATION','build/validation/regressions04')/'launcher'/args.game;out.mkdir(parents=True,exist_ok=True)
script=out/'drive.input';script.write_text('WAIT 900\nPRESS START 2\nWAIT 240\nPRESS START 2\nWAIT 600\nHOLD B\nWAIT 660\nEXIT\n')
prefs=ROOT/'.local/launcher.ini';before=prefs.read_bytes()
logs=ROOT/'.local/logs';logs.mkdir(exist_ok=True);old=set(logs.glob('*.log'))
env=os.environ.copy();env.update(RR_QA_INPUT=str(script),RR_QA_GAME=args.game[-1],RR_GEOMETRY_STATS=str(out/'geometry.json'),RR_PRESENT_CAPTURE_DIR=str(out),RR_PRESENT_CAPTURE_FRAME='2340')
(out/'present.png').unlink(missing_ok=True)
r=subprocess.run([str(ROOT/'build/release02/bin/TrilogyLauncher.exe'),'--qa-launch-crt'],env=env,cwd=ROOT,creationflags=subprocess.CREATE_NO_WINDOW,timeout=100)
assert r.returncode==0,r.returncode
assert prefs.read_bytes()==before,'Launcher preferences changed during QA'
created=set(logs.glob('*.log'))-old;assert len(created)==1,created
log=created.pop();content=log.read_text(errors='replace')
for marker in [f'Road Rash Trilogy {version}','wide=1 | geometry=1 | filter=2',f'version={version} requested_wide=1','source=426x224','shader=active','[RR GEOMETRY] object geometry enabled']:
    assert marker in content,marker
assert (out/'present.png').exists()
stats=json.loads((out/'geometry.json').read_text())
assert stats['intermediate_objects']>100 and stats['source_objects']>100 and stats['terrain_pixels']>10000,stats
(out/'result.json').write_text(json.dumps(dict(game=args.game,exit=r.returncode,log=str(log.relative_to(ROOT)),preferences_unchanged=True,source_width=426,crt=True,geometry=stats),indent=2))
print(f'Launcher -> {args.game}: correct executable, 16:9, koko, continuous geometry and unchanged preferences; exit 0')
print(log)
