"""Create an auditable public source tree and ROM-free installer packages."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import zipfile

ROOT=Path(__file__).resolve().parents[1]
VERSION='0.50'
DIRECTORIES=('src','tools','scripts','games','patches','docs','config','tests','music','.github')
TOP_FILES=('CMakeLists.txt','.gitignore','README.md','CREDITS.md','THIRD_PARTY_NOTICES.md',
           'CONTRIBUTING.md','AGENTS.md','PRODUCT.md','DESIGN.md','Jogar.cmd','rr.ps1')
FORBIDDEN={'.local','.tools','build','dist','.git','__pycache__'}
AUDIO_EXTENSIONS={'.m4a','.mp3','.wav','.aac','.ogg','.flac'}

def music_files(pack):
    """Accept only mapped full tracks with matching provenance, never raw downloads."""
    pack=Path(pack).resolve()
    provenance=json.loads((pack/'sources.json').read_text(encoding='utf-8'))
    records={(row['game'],int(row['cue'])):row for row in provenance}
    files={Path('sources.json')}
    audio=set();cues=0
    for game in ('rr1','rr2','rr3'):
        manifest=Path(game+'.tsv');files.add(manifest);seen=set()
        for line in (pack/manifest).read_text(encoding='utf-8').splitlines():
            cue,filename,title=line.split('\t',2);cue=int(cue)
            if cue in seen or not title:raise ValueError(f'Invalid music cue: {game}/{cue}')
            seen.add(cue)
            if '/' in filename or '\\' in filename or Path(filename).suffix.lower() not in ('.m4a','.mp3','.wav','.aac'):
                raise ValueError(f'Unsafe music filename: {filename}')
            relative=Path('source')/filename;source=(pack/relative).resolve()
            if not source.is_relative_to(pack):raise ValueError(f'Unsafe music path: {filename}')
            row=records.get((game,cue),{})
            if not row.get('artist') or not row.get('url','').startswith('https://') or row.get('duration',0)<120:
                raise ValueError(f'Missing full-track provenance: {game}/{cue}')
            if hashlib.sha256(source.read_bytes()).hexdigest()!=row.get('sha256'):
                raise ValueError(f'Music checksum mismatch: {game}/{cue}')
            files.add(relative);audio.add(relative);cues+=1
        if not seen:raise ValueError(f'Empty music manifest: {game}')
    return sorted(files),dict(artist='Dean Harris',tracks=len(audio),cues=cues,path='music/dean-harris')

def public_files(root):
    for name in TOP_FILES:
        p=root/name
        if p.is_file():yield p
    for name in (*DIRECTORIES,'assets/launcher'):
        directory=root/name
        for p in sorted(directory.rglob('*')):
            relative=p.relative_to(root)
            if p.is_file() and not any(part in FORBIDDEN for part in relative.parts) and p.suffix.lower() not in AUDIO_EXTENSIONS and p.suffix.lower() not in ('.pyc','.bin','.mdrom','.gen','.srm','.state','.exe','.dll','.zip') and relative.parts[:2]!=('music','dean-harris'):
                yield p
    # Design-system tokens are development documentation, never desktop captures.
    for name in ('design.json','surfaces/launcher.md'):
        p=root/'.impeccable'/name
        if p.is_file():yield p

def audit(directory, binary=False, offline=False, music=False):
    entries=[]
    for p in sorted(directory.rglob('*')):
        if not p.is_file():continue
        rel=p.relative_to(directory)
        if any(x in FORBIDDEN for x in rel.parts):raise ValueError(f'Private path in package: {rel}')
        ext=p.suffix.lower()
        if ext in ('.bin','.gen','.srm','.state','.pyc','.pdb','.obj'):raise ValueError(f'Private/generated file in package: {rel}')
        if ext in ('.exe','.dll') and (not binary or rel.as_posix() not in ('RoadRashTrilogyRecomp.exe','SDL2.dll')):raise ValueError(f'Unexpected executable: {rel}')
        if ext in ('.zip','.nupkg') and (not offline or tuple(x.lower() for x in rel.parts[:2])!=('tools','downloads')):raise ValueError(f'Unexpected archive: {rel}')
        if ext in AUDIO_EXTENSIONS and (not music or rel.parts[:3]!=('music','dean-harris','source')):raise ValueError(f'Unexpected audio: {rel}')
        if p.name!='PACKAGE-MANIFEST.json':entries.append(dict(path=rel.as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    return entries

def stage(root, destination, launcher=None, offline=False, music_pack=None):
    music_entries,music_info=music_files(music_pack) if music_pack else ([],None)
    if destination.exists():
        raise ValueError(f'Package destination already exists: {destination}. Use a fresh output directory.')
    destination.mkdir(parents=True)
    for source in public_files(root):
        target=destination/source.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    if launcher:
        shutil.copy2(launcher,destination/'RoadRashTrilogyRecomp.exe')
        shutil.copy2(launcher.parent/'SDL2.dll',destination/'SDL2.dll')
    for relative in music_entries:
        target=destination/'music/dean-harris'/relative;target.parent.mkdir(parents=True,exist_ok=True)
        if relative==Path('sources.json'):
            # Raw downloader metadata can contain local paths and request headers.
            rows=json.loads((music_pack/relative).read_text(encoding='utf-8'))
            keys=('game','cue','artist','url','duration','sha256')
            rows=[{key:row[key] for key in keys} for row in rows]
            target.write_text(json.dumps(rows,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
        else:shutil.copy2(music_pack/relative,target)
    if offline:
        lock=json.loads((root/'tools/public-tools.lock.json').read_text())
        for entry in lock['downloads']:
            source=root/'.tools/downloads'/entry['archive']
            if hashlib.sha256(source.read_bytes()).hexdigest()!=entry['sha256']:raise ValueError(f'Offline cache checksum mismatch: {source.name}')
            target=destination/'Tools/downloads'/source.name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    manifest=dict(product='RoadRashTrilogyRecomp',version=VERSION,kind='offline-installer' if offline else 'installer' if launcher else 'source',roms_included=False,music_pack=music_info,files=audit(destination,bool(launcher),offline,bool(music_pack)))
    (destination/'PACKAGE-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    return manifest

def archive(directory):
    target=directory.parent/(directory.name+'.zip')
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(directory.rglob('*')):
            if p.is_file():z.write(p,Path(directory.name)/p.relative_to(directory))
    return target

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'dist/0.50')
    parser.add_argument('--launcher',type=Path,default=ROOT/'build/release02/bin/TrilogyLauncher.exe')
    parser.add_argument('--offline',action='store_true')
    parser.add_argument('--source-only',action='store_true')
    parser.add_argument('--music-pack',type=Path,help='Include verified mapped remake tracks in installer packages only')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    packages=[]
    source=out/'RoadRashTrilogyRecomp-0.50-source';stage(ROOT,source);packages.append(archive(source))
    if not args.source_only:
        installer=out/'RoadRashTrilogyRecomp-0.50-Windows-x64';stage(ROOT,installer,args.launcher.resolve(),music_pack=args.music_pack);packages.append(archive(installer))
        if args.offline:
            installer=out/'RoadRashTrilogyRecomp-0.50-Windows-x64-offline';stage(ROOT,installer,args.launcher.resolve(),True,music_pack=args.music_pack);packages.append(archive(installer))
    (out/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in packages),encoding='utf-8')
    for p in packages:print(f'{p.name}: {p.stat().st_size} bytes')

if __name__=='__main__':main()
