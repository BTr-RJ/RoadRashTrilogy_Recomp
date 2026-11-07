"""Create an auditable public source tree and ROM-free installer packages."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import zipfile

ROOT=Path(__file__).resolve().parents[1]
VERSION='0.50'
TOP_FILES=('CMakeLists.txt','.gitignore','README.md','CREDITS.md','THIRD_PARTY_NOTICES.md',
           'CONTRIBUTING.md','Jogar.cmd')
PUBLIC_TOOLS=('apply_runtime_patches.py','audit_release.py','audit_repository.py',
              'bootstrap_public.py','install_roms.py','package_release.py',
              'patch_controls.py','patch_engine.py','patch_presentation.py',
              'patch_raster.py','patch_raster_timing.py','patch_sprite_geometry.py',
              'prepare_runtime_inputs.py','public-tools.lock.json','rr.py','tools.lock.json')
PUBLIC_DOCS=('BUILDING.md','CONTROLS.md','INSTALLATION.md','RELEASE-0.50.md','VIDEO.md','ROADMAP.md')
PUBLIC_ASSETS=('BARLOW-LICENSE.txt','Barlow-Regular.ttf','BarlowCondensed-Bold.ttf',
               'BarlowCondensed-BoldItalic.ttf','FONT-LICENSE.txt','app.ico',
               'brand-050.png','rr1.png','rr2.png','rr3.png','sources.json')
SOURCE_TYPES={'src':{'.c','.cpp','.h','.hpp','.in','.manifest'},
              'games':{'.json','.toml'},'tests':{'.py','.c','.cpp','.h','.hpp'},
              'music':{'.md','.json'},'.github':{'.md','.yml','.yaml'}}
FORBIDDEN={'.local','.tools','build','dist','.git','__pycache__'}
AUDIO_EXTENSIONS={'.m4a','.mp3','.wav','.aac','.ogg','.flac'}


def is_public_path(relative):
    """Share one explicit source policy between packaging and repository CI."""
    relative=Path(relative);parts=relative.parts;name=relative.as_posix()
    if not parts or any(part in FORBIDDEN for part in parts):return False
    if parts[:2]==('music','dean-harris'):return False
    if name in TOP_FILES:return True
    if len(parts)==2 and parts[0]=='tools':return parts[1] in PUBLIC_TOOLS
    if len(parts)==2 and parts[0]=='scripts':return parts[1] in ('build.ps1','install.ps1')
    if name=='patches/runtime-integration.patch':return True
    if len(parts)==2 and parts[0]=='docs':return parts[1] in PUBLIC_DOCS
    if parts[:2]==('docs','licenses'):return relative.suffix.lower() in ('.md','.txt')
    if parts[:3]==('docs','media','0.50') and len(parts)==4:
        return parts[3] in ('comparison.json','rr1-original-vs-zoom-0.50.gif',
                            'rr2-original-vs-zoom-0.50.gif','rr3-original-vs-zoom-0.50.gif')
    if len(parts)==3 and parts[:2]==('assets','launcher'):return parts[2] in PUBLIC_ASSETS
    return relative.suffix.lower() in SOURCE_TYPES.get(parts[0],set())

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
    for name in (*SOURCE_TYPES,'tools','scripts','patches','docs','assets/launcher'):
        directory=root/name
        for p in sorted(directory.rglob('*')):
            relative=p.relative_to(root)
            if p.is_file() and is_public_path(relative):
                yield p

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
