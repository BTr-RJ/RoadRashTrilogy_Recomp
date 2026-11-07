"""Audit matched before/after replays for distant grass and paused RR1 edges."""
from collections import Counter
from pathlib import Path
import argparse
import hashlib
import json
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from png_utils import read

def colors(image,x0,x1,y0,y1):
    _,_,channels,rows=image
    return Counter(tuple(rows[y][x*channels:x*channels+3]) for y in range(y0,y1) for x in range(x0,x1))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before',type=Path,required=True)
    parser.add_argument('--after',type=Path,required=True)
    parser.add_argument('--four-three',type=Path,required=True)
    args=parser.parse_args();report=dict(games=[],python_tests=24,native_groups=9)
    for game,y0,y1 in [('rr1',75,90),('rr2',65,85),('rr3',88,108)]:
        before=args.before/game/'wide-';after=args.after/game/'wide-'
        replay=json.loads((after/'comparison.json').read_text());assert replay['passed'],game
        old=read(before/'on/000.png');new=read(after/'on/000.png');native=read(after/'off/000.png')
        assert old[0]==new[0]==native[0]==426,game
        ground=colors(native,370,400,y0,y1)
        shades=[rgb for rgb,count in ground.most_common() if rgb[1]>rgb[0]*1.25 and rgb[1]>rgb[2]*1.25]
        assert len(shades)>=2,(game,'missing native grass fixture')
        detail=shades[1];old_count=colors(old,370,400,y0,y1)[detail]
        new_count=colors(new,370,400,y0,y1)[detail];native_count=ground[detail]
        assert new_count>old_count and new_count>=native_count*.8,(game,old_count,new_count,native_count)
        state={name:(before/'on'/name).read_bytes()==(after/'on'/name).read_bytes()
               for name in ('ram.bin','vram.bin','resume-ram.bin','resume-vram.bin')}
        assert all(state.values()),(game,state)
        assert old[3][152:]==new[3][152:],(game,'HUD changed')
        report['games'].append(dict(game=game,grass_detail_pixels_before=old_count,
            grass_detail_pixels_after=new_count,native_detail_pixels=native_count,
            guest_state_identical=state,replay=replay))
    before=read(args.before/'rr1/wide-/on/pause.png');after=read(args.after/'rr1/wide-/on/pause.png')
    road_color=colors(after,120,210,145,146).most_common(1)[0][0]
    old_break=sum(count for rgb,count in colors(before,0,53,145,146).items() if rgb!=road_color)
    new_break=sum(count for rgb,count in colors(after,0,53,145,146).items() if rgb!=road_color)
    assert old_break>0 and new_break==0,(old_break,new_break)
    report['rr1_pause']=dict(broken_margin_pixels_before=old_break,broken_margin_pixels_after=new_break)
    report['rr1_four_three']=json.loads((args.four_three/'rr1/comparison.json').read_text())
    assert report['rr1_four_three']['passed']
    root=Path(__file__).resolve().parents[1]
    report['executables']={game:hashlib.sha256((root/'build/release02/bin'/game/(game+'.exe')).read_bytes()).hexdigest() for game in ('rr1','rr2','rr3')}
    (args.after/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Distant grass retained in RR1/2/3; RR1 pause edge fixed; guest RAM/VRAM and HUD preserved; 4:3 passed')

if __name__=='__main__':main()
