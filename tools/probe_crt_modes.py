"""RR1 actual non-benchmark renderer path, all eight launcher filter choices."""
from pathlib import Path
import argparse,os,subprocess,json
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--wide',default='1');p.add_argument('--filters',default='0,1,2,3,4,5,6,7');p.add_argument('--vsync',default='1');args=p.parse_args()
presets=['','','Monitor-Balanced.slangp','Monitor-Ambilight-immersive.slangp','Base.slangp','','','']
modes=['raw','linear','raw','raw','raw','crt','trinitron','composite'];reports=[]
for i in map(int,args.filters.split(',')):
    mode=modes[i]
    out=ROOT/'build/validation/bugs03/rr1'/f'filter-{i}-wide-{args.wide}';out.mkdir(parents=True,exist_ok=True)
    script=out/'drive.input';script.write_text('WAIT 900\nPRESS START 2\nWAIT 240\nPRESS START 2\nWAIT 600\nHOLD B\nWAIT 660\nEXIT\n')
    env=os.environ.copy();env.update(RR_ROOT=str(ROOT),RR_MUSIC='original',RR_WIDE=args.wide,RR_TEST_HIDDEN='1',RR_OUTPUT_WIDTH='1280',RR_OUTPUT_HEIGHT='720',SDL_AUDIODRIVER='dummy',SDL_RENDER_VSYNC=args.vsync,GENESIS_SCREEN=mode,RR_PRESENT_CAPTURE_DIR=str(out),RR_PRESENT_CAPTURE_FRAME='2340',RR_SHADER_PATH=str(ROOT/'assets/shaders/koko-aio/Presets-ng'/presets[i]) if presets[i] else '')
    (out/'present.png').unlink(missing_ok=True)
    with (out/'run.log').open('w') as log:
        r=subprocess.run([str(ROOT/'build/release02/bin/rr1/rr1.exe'),str(ROOT/'.local/build-inputs/rr1/game.bin'),'--no-launcher','--turbo','--max-frames','2450','--pacing','accurate','--input-script',str(script)],env=env,cwd=out,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW,timeout=80)
    logtext=(out/'run.log').read_text(errors='replace');assert 'parse error' not in logtext
    report=dict(filter=i,wide=int(args.wide),vsync=int(args.vsync),exit=r.returncode,capture=(out/'present.png').exists());reports.append(report);print(json.dumps(report),flush=True)
    for line in (out/'run.log').read_text(errors='replace').splitlines():
        if '[RR PRESENT]' in line or '[RR SHADER]' in line or '[VIDEO] renderer' in line:print(line,flush=True)
    if r.returncode:break
(ROOT/f'build/validation/bugs03/rr1/filters-wide-{args.wide}.json').write_text(json.dumps(reports,indent=2))
raise SystemExit(any(r['exit'] or not r['capture'] for r in reports))
