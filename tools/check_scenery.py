"""Check scenery-copy recovery against matched racing/pause/resume captures."""
from pathlib import Path
from collections import Counter
import argparse
import csv
import hashlib
import json
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from png_utils import read

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--four-three',type=Path,required=True)
    a=p.parse_args();report=dict(games=[],native_test_groups=9)
    for game in ('rr1','rr2','rr3'):
        old=a.before/game/'wide-';new=a.after/game/'wide-'
        result=json.loads((new/'comparison.json').read_text());assert result['passed'],game
        guest={name:(old/'on'/name).read_bytes()==(new/'on'/name).read_bytes()
               for name in ('ram.bin','vram.bin','resume-ram.bin','resume-vram.bin')}
        assert all(guest.values()),(game,guest)
        for name in ('title.png','pause.png'):
            assert read(old/'on'/name)==read(new/'on'/name),(game,name)
        assert read(old/'on/000.png')[3][152:]==read(new/'on/000.png')[3][152:],(game,'HUD')
        def groups(root):
            with (root/'on/objects.csv').open() as f:
                return Counter((r['frame'],r['key']) for r in csv.DictReader(f) if r['retained']=='0')
        previous=groups(old);current=groups(new)
        restored=sum(n>2 for n in current.values());assert restored>0,game
        assert max(previous.values())<=2,(game,'wrong old renderer fixture')
        with (a.after/(game+'-audit.csv')).open() as f:
            audit=[r for r in csv.DictReader(f) if r.get('replacement')]
        end=max(int(r['frame']) for r in audit)
        resumed=[r for r in audit if int(r['frame'])>=end-100 and r['replacement']=='1' and int(r['copies'])>1]
        assert resumed,(game,'no recovered scenery copies after resume')
        report['games'].append(dict(game=game,replay_passed=True,guest_identical=guest,
            pause_identical=True,hud_identical=True,recovered_multicopy_groups=restored,
            max_billboards_per_group=max(current.values()),resumed_multicopy_updates=len(resumed)))
    narrow=json.loads((a.four_three/'rr3/comparison.json').read_text());assert narrow['passed']
    report['rr3_four_three_passed']=True
    root=Path(__file__).resolve().parents[1]
    report['executables']={g:hashlib.sha256((root/'build/release02/bin'/g/(g+'.exe')).read_bytes()).hexdigest() for g in ('rr1','rr2','rr3')}
    (a.after/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('RR1/2/3 scenery copies recovered during racing and after resume; pause, HUD and guest RAM/VRAM preserved; RR3 4:3 passed')

if __name__=='__main__':main()
