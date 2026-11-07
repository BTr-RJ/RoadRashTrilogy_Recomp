"""Audit captured native player pixels, state/HUD and blue road artifacts."""
from pathlib import Path
import argparse,csv,json,sys,os
sys.path.insert(0,str(Path(__file__).resolve().parent))
from png_utils import read

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game',choices=['rr1','rr2','rr3'])
p.add_argument('--label',default='fixed')
p.add_argument('--scenario',choices=['combat','touring','approach','corner','corner-right','traffic'],default='combat')
p.add_argument('--native-root',default='')
p.add_argument('--report-only',action='store_true')
a=p.parse_args();base=ROOT/os.environ.get('RR_VISUAL_VALIDATION','build/validation/regressions04')/a.game
native=base/'native'/a.scenario;target=base/a.label/a.scenario
if a.native_root:native=ROOT/a.native_root/a.game/'native'/a.scenario
player={'rr1':'FFD37C','rr2':'FFCEB8','rr3':'FF371C'}[a.game]
jobs=list(csv.DictReader((native/'jobs.csv').open()))
jobs=[r for r in jobs if r['job']==player and r['pieces']]
# H40 committed SAT location, verified for the pinned three ROM profiles.
SAT=0xc400
def word(data,at):return int.from_bytes(data[at:at+2],'big')
def matching(job,vram):
    pieces=bytes.fromhex(job['pieces']);first=int(job['first'])
    return all(vram[SAT+first*8+n*8+k]==pieces[n*8+k]
        for n in range(len(pieces)//8) for k in (0,1,2,4,5,6,7))
def pixels(piece,vram,margin):
    y=(word(piece,0)&511)-128;x=(word(piece,6)&511)-128+margin
    w=((piece[2]>>2)&3)+1;h=(piece[2]&3)+1;attr=word(piece,4)
    for dy in range(h*8):
        if not 0<=y+dy<152:continue
        sy=h*8-1-dy if attr&0x1000 else dy
        for dx in range(w*8):
            sx=w*8-1-dx if attr&0x0800 else dx
            tile=((attr&2047)+(sx>>3)*h+(sy>>3))&2047
            byte=vram[(tile*32+(sy&7)*4+(sx&7)//2)&65535]
            if (byte>>(0 if sx&1 else 4))&15:yield x+dx,y+dy
def exclusions(vram,width):
    boxes=[];slot=0;seen=set();margin=(width-320)//2
    while slot not in seen and slot<80:
        seen.add(slot);piece=vram[SAT+slot*8:SAT+slot*8+8]
        x=(word(piece,6)&511)-128+margin;y=(word(piece,0)&511)-128
        w=(((piece[2]>>2)&3)+1)*8;h=((piece[2]&3)+1)*8
        boxes.append((x-12,y-4,x+w+12,y+h+4))
        slot=piece[3]&127
        if not slot:break
    return boxes
report=dict(game=a.game,label=a.label,scenario=a.scenario,frames=0,
    matched_player_frames=0,player_pixels=0,changed_player_pixels=0,
    blue_road_pixels=0,ram_identical=True,vram_identical=True,hud_identical=True)
worst=[]
audit_path=target/'raster-audit.csv'
raster_path=target/'raster-audit.csv.json'
for source in sorted(native.glob('[0-9][0-9][0-9].png')):
    n=int(source.stem);tick={'combat':2090+2*n,'touring':2340+150*n,'approach':2440+n,'corner':2440+n,'corner-right':2440+n,'traffic':9300+n}[a.scenario]
    original=read(source);fixed=read(target/source.name);w,h,c,rows=original
    assert original[:3]==fixed[:3]
    if a.game=='rr1' and a.scenario=='corner' and n==0:
        # This replay starts with A scroll pinned at 320: the old left edge
        # makes a vertical grass strip in the expanded view. It must continue
        # off-screen rather than leave that strip below the perspective edge.
        restored=0
        for y in range(136,152):
            for x in range(20):
                old=rows[y][x*c:x*c+3];new=fixed[3][y][x*c:x*c+3]
                restored+=old[1]>old[0] and old[1]>old[2] and max(new)-min(new)<=2 and 12<new[0]<130
        report['corner_extension_pixels']=restored
    report['frames']+=1
    report['hud_identical'] &= rows[152:]==fixed[3][152:]
    for ext,field in [('ram','ram_identical'),('vram','vram_identical')]:
        report[field] &= source.with_suffix('.'+ext).read_bytes()==(target/source.with_suffix('.'+ext).name).read_bytes()
    vram=source.with_suffix('.vram').read_bytes()
    job=next((r for r in reversed(jobs) if int(r['tick'])<=tick and matching(r,vram)),None)
    if job and int(job['scale'])==16384:
        report['matched_player_frames']+=1
        pieces=bytes.fromhex(job['pieces']);mask=set()
        for at in range(0,len(pieces),8):mask.update(pixels(pieces[at:at+8],vram,(w-320)//2))
        changed=0
        for x,y in mask:
            if 0<=x<w:
                report['player_pixels']+=1
                changed+=rows[y][x*c:(x+1)*c]!=fixed[3][y][x*c:(x+1)*c]
        report['changed_player_pixels']+=changed
        if changed:worst.append((changed,n))
    if raster_path.exists():continue
    boxes=exclusions(vram,w)
    for y in range(72,151):
        for x in range(8,w-8):
            old=rows[y][x*c:x*c+3];new=fixed[3][y][x*c:x*c+3]
            if max(old)-min(old)>3 or not 12<old[0]<130:continue
            if new[2]>130 and new[0]<100 and new[1]<110 and new[2]>new[1]*1.4:
                if not any(l<=x<r and t<=y<b for l,t,r,b in boxes):report['blue_road_pixels']+=1
report['worst_player_frames']=sorted(worst,reverse=True)[:8]
report['passed']=all(report[k] for k in ('ram_identical','vram_identical','hud_identical'))
audit=raster_path
if audit.exists():
    # DMA may upload the next animation after its scanlines have been drawn.
    # Use the actual raster owners/colors for the strict player assertion.
    report['end_of_frame_mask_differences']=report.pop('changed_player_pixels')
    report['raster']=json.loads(audit.read_text())
    report['passed'] &= report['raster']['player_pixels']>100000
    report['blue_road_pixels']=report['raster']['blue_road_pixels']
    report['passed'] &= report['raster']['corrupted_player_pixels']==0
    report['passed'] &= report['raster']['missing_actor_pixels']==0
else:
    report['passed'] &= report['matched_player_frames']>=report['frames']//2
    report['passed'] &= report['changed_player_pixels']==0
report['passed'] &= report['blue_road_pixels']==0
if a.game=='rr1' and a.scenario=='corner':
    report['passed'] &= report.get('corner_extension_pixels',0)>=150
(target/'pixel-audit.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2),flush=True)
if not a.report_only and not report['passed']:raise SystemExit(1)
