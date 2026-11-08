"""Verify final ZIP names, checksums, file manifests and ROM exclusion."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile
import re

ROOT=Path(__file__).resolve().parents[1]
VERSION=re.search(r'project\(RoadRashTrilogyRecomp VERSION ([0-9.]+)',(ROOT/'CMakeLists.txt').read_text())[1]

def audit(output):
    rom_hashes={entry['sha256'] for entry in json.loads((ROOT/'games/supported-roms.json').read_text()).values()}
    rows=[]
    for line in (output/'SHA256SUMS.txt').read_text().splitlines():
        expected,name=line.split('  ',1);path=output/name
        assert hashlib.sha256(path.read_bytes()).hexdigest()==expected,name
        assert name.startswith(f'RoadRashTrilogyRecomp-{VERSION}-'),name
        with zipfile.ZipFile(path) as z:
            manifest_name=next(n for n in z.namelist() if n.endswith('/PACKAGE-MANIFEST.json'))
            prefix=manifest_name.removesuffix('PACKAGE-MANIFEST.json');manifest=json.loads(z.read(manifest_name))
            assert manifest['version']==VERSION and manifest['roms_included'] is False,name
            files={e['path']:e for e in manifest['files']}
            actual={n.removeprefix(prefix) for n in z.namelist() if not n.endswith('/') and n!=manifest_name}
            assert actual==set(files),(name,'manifest membership')
            for relative,e in files.items():
                data=z.read(prefix+relative);digest=hashlib.sha256(data).hexdigest()
                assert digest==e['sha256'] and len(data)==e['bytes'],(name,relative)
                assert digest not in rom_hashes,(name,'commercial ROM')
                parts=Path(relative).parts
                assert not any(p.lower() in ('.local','.tools','build','__pycache__') for p in parts),(name,relative)
                assert Path(relative).suffix.lower() not in ('.bin','.gen','.srm','.state','.pyc','.pdb','.obj'),(name,relative)
                if Path(relative).suffix.lower() in ('.m4a','.mp3','.wav','.aac','.ogg','.flac'):
                    assert manifest.get('music_pack') and relative.startswith('music/dean-harris/source/'),(name,relative)
            pack=manifest.get('music_pack')
            if pack:
                assert manifest['kind']!='source',(name,'audio in source package')
                base=prefix+pack['path']+'/'
                provenance={(r['game'],int(r['cue'])):r for r in json.loads(z.read(base+'sources.json'))}
                tracks=set();cue_count=0
                for game in ('rr1','rr2','rr3'):
                    seen=set()
                    for line in z.read(base+game+'.tsv').decode('utf-8').splitlines():
                        cue,filename,title=line.split('\t',2);cue=int(cue)
                        assert cue not in seen and title and '/' not in filename and '\\' not in filename,(name,game,cue)
                        seen.add(cue);row=provenance[(game,cue)]
                        assert row['duration']>=120 and row['artist'] and row['url'].startswith('https://'),(name,game,cue)
                        assert hashlib.sha256(z.read(base+'source/'+filename)).hexdigest()==row['sha256'],(name,filename)
                        tracks.add(filename);cue_count+=1
                    assert seen,(name,game)
                assert len(tracks)==pack['tracks'] and cue_count==pack['cues'],(name,'music counts')
                assert {r.removeprefix(pack['path']+'/source/') for r in files if r.startswith(pack['path']+'/source/')}==tracks,(name,'unmapped audio')
        rows.append(dict(file=name,bytes=path.stat().st_size,sha256=expected,files=len(files),kind=manifest['kind'],roms_included=False,music_pack=manifest.get('music_pack')))
    assert len(rows)==len({r['file'] for r in rows}), 'Archive names collided'
    return rows

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);args=p.parse_args()
    rows=audit(args.output.resolve());print(json.dumps(dict(version=VERSION,packages=rows),indent=2))
