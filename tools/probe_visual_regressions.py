"""Capture combat, nearby riders, traffic and hills with real controller inputs."""
from pathlib import Path
import argparse,json,os,shutil,subprocess,uuid,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from png_utils import read,write

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game',choices=['rr1','rr2','rr3'])
p.add_argument('--build',default='build/release02')
p.add_argument('--label',default='fixed')
p.add_argument('--scenario',choices=['combat','touring','approach','corner','corner-right','traffic','dynamics','rivals','grid'],default='combat')
p.add_argument('--native',action='store_true')
p.add_argument('--wait',type=int,default=None,help='Override the initial racing wait for a focused regression')
p.add_argument('--steer',choices=['left','right'],help='Hold steering during the initial racing wait')
a=p.parse_args()
out=ROOT/os.environ.get('RR_VISUAL_VALIDATION','build/validation/regressions04')/a.game/a.label/a.scenario
out.mkdir(parents=True,exist_ok=True)
prefix='r'+uuid.uuid4().hex[:8]
ops=['WAIT 900','PRESS START 2','WAIT 240','PRESS START 2','WAIT 600','HOLD B']
files=[]
if a.steer:ops+=['HOLD '+a.steer.upper()]
ops+=['WAIT '+str(a.wait if a.wait is not None else {'combat':350,'touring':600,'approach':700,'corner':700,'corner-right':700,'traffic':7560,'dynamics':900,'rivals':500,'grid':0}[a.scenario])]
if a.steer:ops+=['RELEASE '+a.steer.upper()]
count={'combat':120,'touring':48,'approach':240,'corner':240,'corner-right':240,'traffic':480,'dynamics':480,'rivals':480,'grid':480}[a.scenario]
for n in range(count):
    if a.scenario=='rivals':
        if n%60==0:ops+=['PRESS C 30']
        if n%48==0:
            ops+=['RELEASE LEFT','RELEASE RIGHT']
            if (n//48)%3!=2:ops+=['HOLD '+('LEFT' if (n//48)%3==0 else 'RIGHT')]
    if a.scenario=='dynamics':
        if n%24==0:
            ops+=['RELEASE LEFT','RELEASE RIGHT','HOLD '+('LEFT' if (n//24)%2==0 else 'RIGHT')]
        if n%96==48:ops+=['RELEASE B','HOLD A']
        if n%96==72:ops+=['RELEASE A','HOLD B']
    if a.scenario=='combat':
        if n in (16,48,80):ops+=['PRESS C 48']
        if n==48:ops+=['HOLD UP']
        if n==64:ops+=['RELEASE UP']
        if n==80:ops+=['HOLD LEFT']
        if n==96:ops+=['RELEASE LEFT','HOLD RIGHT']
        if n==112:ops+=['RELEASE RIGHT']
    if a.scenario=='corner':
        if n==16:ops+=['HOLD LEFT']
        if n==80:ops+=['RELEASE LEFT','HOLD RIGHT']
        if n==160:ops+=['RELEASE RIGHT','HOLD LEFT']
        if n==208:ops+=['RELEASE LEFT']
    if a.scenario=='corner-right':
        if n==16:ops+=['HOLD RIGHT']
        if n==80:ops+=['RELEASE RIGHT','HOLD LEFT']
        if n==160:ops+=['RELEASE LEFT','HOLD RIGHT']
        if n==208:ops+=['RELEASE RIGHT']
    for command,ext in [('SCREENSHOT','png'),('DUMP_RAM','ram'),('DUMP_VRAM','vram')]:
        name=f'{prefix}_{n:03d}.{ext}';files.append((name,f'{n:03d}.{ext}'))
        ops+=[command+' '+name]
    ops+=['WAIT '+str({'combat':2,'touring':150,'approach':1,'corner':1,'corner-right':1,'traffic':1,'dynamics':1,'rivals':1,'grid':1}[a.scenario])]
ops+=['RELEASE','EXIT']
script=out/'drive.input';script.write_text('\n'.join(ops)+'\n')
env=os.environ.copy();env.update(RR_ROOT=str(ROOT),RR_GEOMETRY='0' if a.native else '1',
    RR_WIDE='1',RR_ORGANIC='1',RR_MUSIC='original',RR_TEST_HIDDEN='1',
    RR_OUTPUT_WIDTH='960',RR_OUTPUT_HEIGHT='540',RR_SHADER_PATH='',GENESIS_SCREEN='raw',
    SDL_RENDER_VSYNC='0',SDL_AUDIODRIVER='dummy',RR_GEOMETRY_TRACE=str(out/'geometry.csv'),
    RR_JOB_TRACE=str(out/'jobs.csv'),RR_BILLBOARD_TRACE=str(out/'objects.csv'),
    RR_SCENE_TRACE=str(out/'actors.csv'),RR_GEOMETRY_STATS=str(out/'geometry.json'),
    RR_GEOMETRY_AUDIT=str(out/'raster-audit.csv'),RR_TRAFFIC_TRACE=str(out/'traffic.csv'),RR_LANE_TRACE=str(out/'lane.csv'))
env['RR_VEHICLE_COVERAGE']=str(out/'vehicle-pixels.csv')
env['RR_ACTOR_PACKETS']=str(out/'actor-packets.csv')
exe=ROOT/a.build/'bin'/a.game/(a.game+'.exe')
with (out/'run.log').open('w') as log:
    subprocess.run([str(exe),str(ROOT/'.local/build-inputs'/a.game/'game.bin'),
        '--no-launcher','--benchmark','11000','--pacing','accurate','--input-script',str(script)],
        env=env,cwd=out,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,
        creationflags=subprocess.CREATE_NO_WINDOW)
for source,target in files:
    shutil.move(str(exe.parent/source),str(out/target))
    if target.endswith('.png'):
        w,h,c,rows=read(out/target);write(out/target,w,h,rows,c)
assert 'parse error' not in (out/'run.log').read_text(errors='replace')
(out/'capture.json').write_text(json.dumps(dict(game=a.game,scenario=a.scenario,frames=count,
    input_buttons=['B','C','UP','LEFT','RIGHT'] if a.scenario=='combat' else ['B'],native=a.native),indent=2))
print(out,flush=True)
