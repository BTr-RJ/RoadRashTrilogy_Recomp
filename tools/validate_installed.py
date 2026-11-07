"""Verify an isolated local installation via the production launcher path."""
from pathlib import Path
import argparse
import json
import os
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--installation',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();root=args.installation.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    marker=json.loads((root/'.local/installation.json').read_text(encoding='utf-8-sig'))
    results=[];prefs=root/'.local/launcher.ini';before=prefs.read_bytes() if prefs.exists() else None
    for game in marker['games']:
        folder=out/game;folder.mkdir(exist_ok=True);script=folder/'drive.input'
        script.write_text('WAIT 900\nPRESS START 2\nWAIT 240\nPRESS START 2\nWAIT 600\nHOLD B\nWAIT 660\nEXIT\n')
        logs=root/'.local/logs';logs.mkdir(exist_ok=True);old=set(logs.glob('*.log'))
        env=os.environ.copy();env.update(RR_QA_INPUT=str(script),RR_QA_GAME=game[-1],RR_GEOMETRY_STATS=str(folder/'geometry.json'),RR_PRESENT_CAPTURE_DIR=str(folder),RR_PRESENT_CAPTURE_FRAME='2340')
        r=subprocess.run([str(root/'RoadRashTrilogyRecomp.exe'),'--qa-launch-crt'],env=env,cwd=root,creationflags=subprocess.CREATE_NO_WINDOW,timeout=120)
        assert r.returncode==0,(game,r.returncode)
        created=set(logs.glob('*.log'))-old;assert len(created)==1,(game,created)
        log=created.pop();content=log.read_text(errors='replace')
        for text in ('Road Rash Trilogy 0.50','wide=1 | geometry=1 | filter=2','version=0.50 requested_wide=1','source=426x224','shader=active','[RR GEOMETRY] object geometry enabled'):
            assert text in content,(game,text)
        stats=json.loads((folder/'geometry.json').read_text());assert stats['intermediate_objects']>100 and stats['terrain_pixels']>10000,(game,stats)
        assert (folder/'present.png').exists(),game
        results.append(dict(game=game,exit=r.returncode,crt=True,continuous_geometry=True,geometry=stats))
        print(f'{game}: isolated launcher, CRT, widescreen and geometry passed',flush=True)
    assert (prefs.read_bytes() if prefs.exists() else None)==before,'QA changed launcher preferences'
    (out/'result.json').write_text(json.dumps(dict(version='0.50',games=results,preferences_unchanged=True),indent=2)+'\n')

if __name__=='__main__':main()
