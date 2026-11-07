"""Reproducible hidden original/wide captures, using independent game processes."""
from pathlib import Path
import os, subprocess, argparse, shutil
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--game',default='rr3');p.add_argument('--build',default='build/release02');p.add_argument('--wide',default='1');p.add_argument('--shader',default='');p.add_argument('--label',default='probe');args=p.parse_args()
out=ROOT/'build/validation/video-fix'/args.game/args.label;out.mkdir(parents=True,exist_ok=True)
ops=['WAIT 900','SCREENSHOT title.png','PRESS START 2','WAIT 240','SCREENSHOT menu.png','PRESS START 2','WAIT 600','HOLD B']
for n in range(6):
    ops+=['WAIT 300',f'SCREENSHOT race{n}.png',f'DUMP_RAM ram{n}.bin',f'DUMP_VRAM vram{n}.bin']
ops+=['RELEASE B','PRESS START 2','WAIT 90','SCREENSHOT pause.png','EXIT']
script=out/'drive.input';script.write_text('\n'.join(ops)+'\n')
env=os.environ.copy();env.update(RR_ROOT=str(ROOT),RR_WIDE=args.wide,RR_SMOOTH='1',RR_MUSIC='original',RR_TRACE='1',RR_PRESENT_PROBE='1',RR_OUTPUT_WIDTH='1280',RR_OUTPUT_HEIGHT='720',RR_SHADER_PATH=args.shader)
if args.shader:env.update(RR_GPU_CAPTURE=str(out/'crt.png'),RR_GPU_CAPTURE_FRAME='2340')
exe=ROOT/args.build/'bin'/args.game/(args.game+'.exe')
with (out/'run.log').open('w') as log:
    result=subprocess.run([str(exe),str(ROOT/'.local/build-inputs'/args.game/'game.bin'),'--no-launcher','--benchmark','3900','--pacing','accurate','--input-script',str(script)],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW,timeout=100)
for filename in ['title.png','menu.png','pause.png']+[f'{kind}{n}.{ext}' for n in range(6) for kind,ext in [('race','png'),('ram','bin'),('vram','bin')]]:
    source=exe.parent/filename
    if source.exists():shutil.copy2(source,out/filename)
print(args.game,args.label,'exit',result.returncode,flush=True)
raise SystemExit(result.returncode)
