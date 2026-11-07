from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
for game,pointer in [('rr1',0x389c),('rr2',0x2796),('rr3',0x276e)]:
    path=ROOT/f'build/native/bin/{game}/probe_ram.bin'
    data=path.read_bytes()
    def word(off):return struct.unpack_from('>H',data,off&65535)[0]
    base=struct.unpack_from('>I',data,pointer)[0]&65535
    print(game,'buffer',hex(base),'header',[hex(word(base+x)) for x in range(0,20,2)])
    player={'rr1':0x1eaa,'rr2':0xa62,'rr3':0x9b2}[game]
    print('rider',[(i,hex(word(player+i))) for i in [0,4,8,12,16,18,20,22,26,34,38,48,50,54,104,110]])
    offset=word(base+10)
    for i in range(12):
        if offset>0x400 or offset&1:break
        values=[word(base+offset+x) for x in range(0,12,2)]
        print(hex(offset),[hex(v) for v in values])
        if offset==values[-1]:break
        offset=values[-1]
