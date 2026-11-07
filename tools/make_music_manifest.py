"""Map public full-length artist tracks; previews never enter playback."""
from pathlib import Path
import json,hashlib
ROOT=Path(__file__).resolve().parents[1]
destination=ROOT/'.local/music/dean-harris'
# D0=1 is title for both 68K drivers, verified by native boot traces.
# Course IDs are verified separately against the ROM tables before enabling.
mapping={
 'rr1':{1:'pQZlJWA-ZIU',2:'5iqAfBaJoPs',3:'mm03oF4mpOA',4:'SEL48m8w-1M',5:'SEL48m8w-1M',7:'GVzz9Qk42CM',9:'GVzz9Qk42CM'},
 'rr2':{1:'iGYa_2iESQM',2:'g4Y8tELo-ug',3:'YAefuFFvXFw',4:'mYVOxCgihN0',5:'6ogiL63hUX8',6:'jj9ZIjmHT44'},
 'rr3':{1:'glfqF_M4InE',2:'MCBdb02egEY',3:'kGcdAneB044',4:'1hgpf4M9qdM',5:'AXFfbNRi7Wk',6:'lDkOezW6DTI',7:'SiA9mQfCn1A'}
}
import struct
for game,offset,expected in [('rr1',0xa720,[2,3,7,5,4]),('rr2',0xc190,[3,4,6,5,2]),('rr3',0x10bf8,[4,6,7,5,2,3,1])]:
    rom=(ROOT/f'.local/build-inputs/{game}/game.bin').read_bytes()
    actual=struct.unpack_from('>'+str(len(expected))+'H',rom,offset)
    if list(actual)!=expected:raise ValueError('Unexpected course/music table: '+game)
    if game=='rr1' and rom[0x62ba:0x62cc]!=bytes.fromhex('203c0000000933c000ff05d84eb900019f6a'):
        raise ValueError('Unexpected RR1 results music request')
provenance=[]
for game,cues in mapping.items():
    lines=[]
    for cue,video in cues.items():
        meta=json.loads((destination/'source'/(video+'.info.json')).read_text(encoding='utf-8'))
        audio=destination/'source'/(video+'.m4a')
        if meta.get('channel_id')!='UCy61-uPYi7Y3zPdkktRMmzQ' or meta['duration']<120:raise ValueError('Expected full-length artist track')
        lines.append(f'{cue}\t{audio.name}\t{meta["title"]}')
        provenance.append({'game':game,'cue':cue,'artist':meta['channel'],'url':meta['webpage_url'],'duration':meta['duration'],'sha256':hashlib.sha256(audio.read_bytes()).hexdigest()})
    (destination/(game+'.tsv')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
(destination/'sources.json').write_text(json.dumps(provenance,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
print('Local playback manifests written. See sources.json for artist provenance.')
