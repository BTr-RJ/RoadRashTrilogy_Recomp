from pathlib import Path
import struct
for game in ('rr1','rr2','rr3'):
    root=Path(__file__).resolve().parents[1]/'build/validation/bugs03'/game/'audio'
    states=[(root/(label+'.bin')).read_bytes() for label in ('before','paused','resumed')]
    print(game)
    lo,hi=(0x3680,0x36a0) if game=='rr1' else (0x2500,0x2560)
    for addr in range(lo,hi,2):
        words=[struct.unpack_from('>H',s,addr)[0] for s in states]
        print(hex(addr),[hex(w) for w in words])
