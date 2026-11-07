"""Exercise real game pause twice and capture the actual mixed PCM stream."""
from pathlib import Path
import os,subprocess,argparse,shutil,struct,re,json
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--music-volume',default='20');args=p.parse_args();game=args.game
out=ROOT/'build/validation/bugs03'/game/('audio' if args.music_volume=='20' else 'audio-fx');out.mkdir(parents=True,exist_ok=True)
script=out/'drive.input';script.write_text('WAIT 900\nPRESS START 12\nWAIT 240\nPRESS START 12\nWAIT 600\nHOLD B\nWAIT 400\nRELEASE\nWAIT 30\nDUMP_RAM qa_before.bin\nPRESS START 12\nWAIT 120\nDUMP_RAM qa_paused.bin\nPRESS START 12\nWAIT 180\nDUMP_RAM qa_resumed.bin\nPRESS START 12\nWAIT 120\nPRESS START 12\nWAIT 180\nEXIT\n')
env=os.environ.copy();env.update(RR_ROOT=str(ROOT),RR_MUSIC='dean-harris',RR_MUSIC_VOLUME=args.music_volume,RR_EFFECTS_VOLUME='100',RR_TEST_HIDDEN='1',RR_OUTPUT_WIDTH='1280',RR_OUTPUT_HEIGHT='720',RR_WIDE='1',RR_SHADER_PATH='',SDL_AUDIODRIVER='dummy',SDL_RENDER_VSYNC='0',RR_TRACE='1')
exe=ROOT/'build/release02/bin'/game/(game+'.exe')
for label in ['before','paused','resumed']:
    (exe.parent/f'qa_{label}.bin').unlink(missing_ok=True)
with (out/'run.log').open('w') as log:
    result=subprocess.run([str(exe),str(ROOT/'.local/build-inputs'/game/'game.bin'),'--no-launcher','--turbo','--max-frames','3100','--pacing','accurate','--input-script',str(script),'--wav',str(out/'mix.wav')],env=env,cwd=out,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW,timeout=100)
for label in ['before','paused','resumed']:
    src=exe.parent/f'qa_{label}.bin'
    if src.exists():shutil.copy2(src,out/(label+'.bin'))
assert 'parse error' not in (out/'run.log').read_text(errors='replace')
assert all((exe.parent/f'qa_{label}.bin').exists() for label in ['before','paused','resumed'])
print(game,'pause test exit',result.returncode,flush=True)
for line in (out/'run.log').read_text(errors='replace').splitlines():
    if '[RR MUSIC]' in line:print(line,flush=True)
addr={'rr1':0x3696,'rr2':0x254e,'rr3':0x2548}[game]
flags=[struct.unpack_from('>H',(out/(label+'.bin')).read_bytes(),addr)[0] for label in ['before','paused','resumed']]
print('Native pause toggle:',flags,flush=True)
assert flags==[0,1,0],flags
events=re.findall(r'\[RR MUSIC\] (pause|resume) cue=(\d+) position=([\d.]+)',(out/'run.log').read_text(errors='replace'))
pairs=[];pending=None
for action,cue,pos in events:
    if action=='pause':pending=(cue,float(pos))
    elif pending and pending[0]==cue:
        assert abs(pending[1]-float(pos))<.002,(pending,pos)
        if pending[1]>0:pairs.append(dict(cue=cue,position=pending[1]))
        pending=None
assert len(pairs)>=2,pairs
(out/'result.json').write_text(json.dumps(dict(exit=result.returncode,pause_flags=flags,resume_pairs=pairs,music_volume=int(args.music_volume)),indent=2))
raise SystemExit(result.returncode)
