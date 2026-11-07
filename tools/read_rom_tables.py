from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
for game,at,n in [('rr1',0xa720,10),('rr2',0xc190,10),('rr3',0x10bf8,12)]:
    data=(ROOT/f'.local/build-inputs/{game}/game.bin').read_bytes()
    print(game,hex(at),struct.unpack_from('>'+str(n)+'H',data,at))
    names={'rr1':[b'SIERRA',b'PACIFIC',b'REDWOOD',b'PALM',b'GRASS'],'rr2':[b'ALASKA',b'HAWAII',b'ARIZONA',b'TENNESSEE',b'VERMONT'],'rr3':[b'BRAZIL',b'GERMANY',b'ITALY',b'KENYA',b'JAPAN',b'UNITED',b'UK',b'KENIA']}[game]
    for name in names:
        start=data.upper().find(name)
        print(name,hex(start),data[max(0,start-6):start+22])
