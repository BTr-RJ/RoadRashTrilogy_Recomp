"""Prove the real projection adapter observes objects without changing the game."""
from pathlib import Path
import os,subprocess,sys,csv,json
ROOT=Path(__file__).resolve().parents[1]
game=sys.argv[1];out=ROOT/'build/validation/scene-capture';out.mkdir(parents=True,exist_ok=True)
path=out/(game+'.csv');path.unlink(missing_ok=True)
for label,on in [('scene-off',False),('scene-on',True)]:
    env=os.environ.copy();env.pop('RR_SCENE_TRACE',None)
    if on:env['RR_SCENE_TRACE']=str(path)
    subprocess.run([str(ROOT/'.tools/python/python.exe'),str(ROOT/'tools/probe_video.py'),'--game',game,'--wide','0','--label',label],env=env,check=True)
base=ROOT/'build/validation/video-fix'/game
names=['title.png','menu.png','pause.png']+[f'{kind}{n}.{ext}' for n in range(6) for kind,ext in [('race','png'),('ram','bin'),('vram','bin')]]
for name in names:
    assert (base/'scene-off'/name).read_bytes()==(base/'scene-on'/name).read_bytes(),name
rows=list(csv.DictReader(path.open()));assert len(rows)>100,len(rows)
report=dict(game=game,unchanged_files=len(names),projections=len(rows),actor_slots=len({r['actor'] for r in rows}),scale_values=len({r['native_scale'] for r in rows}),projection_ticks=len({r['tick'] for r in rows}))
(out/(game+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
