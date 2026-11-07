"""Compare authentic image centers and full RAM/VRAM for matched drive traces."""
from pathlib import Path
import struct,zlib,json
ROOT=Path(__file__).resolve().parents[1]
def png(path):
    data=path.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n'
    at=8;compressed=b''
    while at<len(data):
        n=struct.unpack_from('>I',data,at)[0];kind=data[at+4:at+8];chunk=data[at+8:at+8+n];at+=12+n
        if kind==b'IHDR':w,h,depth,color,*_=struct.unpack('>IIBBBBB',chunk)
        if kind==b'IDAT':compressed+=chunk
    assert depth==8 and color in (2,6)
    bpp=4 if color==6 else 3;stride=w*bpp;raw=zlib.decompress(compressed);rows=[];prev=bytearray(stride)
    def paeth(a,b,c):
        p=a+b-c;pa,pb,pc=abs(p-a),abs(p-b),abs(p-c)
        return a if pa<=pb and pa<=pc else b if pb<=pc else c
    for y in range(h):
        offset=y*(stride+1);f=raw[offset];row=bytearray(raw[offset+1:offset+1+stride])
        for x in range(stride):
            a=row[x-bpp] if x>=bpp else 0;b=prev[x];c=prev[x-bpp] if x>=bpp else 0
            v=(0,a,b,(a+b)//2,paeth(a,b,c))[f];row[x]=(row[x]+v)&255
        rows.append(row);prev=row
    return w,h,bpp,rows
results=[]
for game in ('rr1','rr2','rr3'):
    base=ROOT/'build/validation/video-fix'/game;native=base/'native';wide=base/'wide'
    for scene in ['title','menu','pause']+[f'race{n}' for n in range(6)]:
        nw,nh,bpp,native_rows=png(native/(scene+'.png'));ww,wh,wbpp,wide_rows=png(wide/(scene+'.png'))
        assert nh==wh and bpp==wbpp and ww>=nw
        margin=(ww-nw)//2
        different=sum(a!=b for nrow,wrow in zip(native_rows,wide_rows) for a,b in zip(nrow,wrow[margin*bpp:(margin+nw)*bpp]))
        results.append(dict(game=game,scene=scene,native_width=nw,wide_width=ww,center_different_bytes=different))
    for n in range(6):
        for kind in ('ram','vram'):
            a=(native/f'{kind}{n}.bin').read_bytes();b=(wide/f'{kind}{n}.bin').read_bytes()
            results.append(dict(game=game,frame=n,kind=kind,different_bytes=sum(x!=y for x,y in zip(a,b))+abs(len(a)-len(b))))
report=ROOT/'build/validation/video-fix/comparison.json';report.write_text(json.dumps(results,indent=2))
bad=[r for r in results if r.get('center_different_bytes',r.get('different_bytes',0))]
for item in bad:print(item)
print(f'{len(results)} comparisons, {len(bad)} failures; {report}')
raise SystemExit(bool(bad))
