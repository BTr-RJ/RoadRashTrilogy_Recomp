from pathlib import Path
import struct
p=Path(__file__).resolve().parents[1]/'assets/launcher/BarlowCondensed-Bold.ttf'
d=p.read_bytes();tables=struct.unpack_from('>H',d,4)[0]
for i in range(tables):
    tag,check,offset,size=struct.unpack_from('>4sIII',d,12+i*16)
    if tag!=b'name':continue
    _,count,strings=struct.unpack_from('>HHH',d,offset)
    for j in range(count):
        platform,encoding,language,name,length,start=struct.unpack_from('>6H',d,offset+6+j*12)
        if platform==3 and language==1033 and name in [1,2,4,6,16,17]:
            print(name,d[offset+strings+start:offset+strings+start+length].decode('utf-16be'))
