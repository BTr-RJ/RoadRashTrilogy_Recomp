"""Hidden enhanced presentation probes; raw and GPU evidence kept separately."""
from pathlib import Path
import json,os,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
for game in ['rr1','rr2','rr3']:
    out=ROOT/'build/validation'/game/'enhanced';out.mkdir(parents=True,exist_ok=True)
    script=out/'drive.input'
    script.write_text('WAIT 900\nPRESS START 2\nWAIT 240\nPRESS START 2\nWAIT 600\nHOLD B\nWAIT 600\nSCREENSHOT enhanced_raw.png\nDUMP_RAM enhanced_ram.bin\nWAIT 600\nSCREENSHOT enhanced_later.png\n')
    env=os.environ.copy();env.update(RR_ROOT=str(ROOT),RR_WIDE='1',RR_SMOOTH='1',RR_MOTION_STATS='1',RR_MUSIC='dean-harris',RR_PRESENT_PROBE='1',
        RR_SHADER_PATH=str(ROOT/'assets/shaders/koko-aio/Presets-ng/Monitor-Balanced.slangp'),
        RR_GPU_CAPTURE=str(out/'koko.png'),RR_GPU_CAPTURE_FRAME='2340')
    binary=ROOT/f'build/native/bin/{game}/{game}.exe'
    command=[str(binary),str(ROOT/f'.local/build-inputs/{game}/game.bin'),'--no-launcher','--benchmark','3100','--pacing','accurate','--input-script',str(script),'--hash-frames','300']
    start=time.monotonic()
    with (out/'run.log').open('w') as log:
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=100,creationflags=subprocess.CREATE_NO_WINDOW)
    report={'game':game,'exitcode':result.returncode,'seconds':round(time.monotonic()-start,3),'gpu_capture':(out/'koko.png').exists()}
    (out/'result.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report),flush=True)
